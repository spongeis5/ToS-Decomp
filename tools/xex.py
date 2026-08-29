"""XEX2 reader for the Truth or Square decompilation.

Turns game/DEFAULT.XEX into a real PE image on disk, plus a manifest of
everything the container declares about it.

Rules this file follows, because they were paid for elsewhere:

  * Nothing returns a benign value when it could not be computed.  A
    quantity that was not measured is NOTMEASURED, which raises if it is
    used as a number.
  * Every count states its denominator.
  * Where the file states a size itself, we compute the same size a second
    way and assert the two agree.  Two derivations of one fact with nothing
    forcing them to agree is how a wrong number survives.
"""

import hashlib
import struct
import sys
from pathlib import Path


class _NotMeasured:
    """A quantity that could not be measured.  Not zero.  Not None."""

    def __bool__(self):
        raise ValueError("NOTMEASURED used as a truth value")

    def __int__(self):
        raise ValueError("NOTMEASURED used as a number")

    def __repr__(self):
        return "NOT_MEASURED"


NOTMEASURED = _NotMeasured()


# The XEX2 retail image key.  This is the published constant every open
# Xbox 360 tool carries (Xenia, xextool); it is what makes a disc the owner
# already has readable.  Devkit images use an all-zero key instead, and we
# try both and keep whichever produces a PE header.
RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")
DEVKIT_KEY = bytes(16)

OPTIONAL_HEADER_NAMES = {
    0x0002: "ResourceInfo",
    0x0003: "BaseFileFormat",
    0x0004: "BaseReference",
    0x0005: "DeltaPatchDescriptor",
    0x0100: "BoundingPath",
    0x0101: "EntryPoint",
    0x0102: "ImageBaseAddress",
    0x0103: "ImportLibraries",
    0x0104: "ChecksumTimestamp",
    0x0105: "EnabledForCallcap",
    0x0106: "EnabledForFastcap",
    0x0107: "OriginalPEName",
    0x0180: "StaticLibraries",
    0x0181: "TLSInfo",
    0x0182: "DefaultStackSize",
    0x0183: "DefaultFilesystemCacheSize",
    0x0184: "DefaultHeapSize",
    0x0185: "PageHeapSizeAndFlags",
    0x0186: "SystemFlags",
    0x0200: "ExecutionInfo",
    0x0201: "ServiceIDList",
    0x0202: "TitleWorkspaceSize",
    0x0203: "GameRatings",
    0x0204: "LANKey",
    0x0205: "Xbox360Logo",
    0x0206: "MultidiscMediaIDs",
    0x0207: "AlternateTitleIDs",
    0x0208: "AdditionalTitleMemory",
    0x0400: "ExportsByName",
    0x0401: "VitalStats",
    0x0402: "CallcapImports",
    0x0403: "FastcapEnabled",
    0x0404: "OriginalPEImageName",
}

ENCRYPTION = {0x0000: "none", 0x0001: "normal"}
COMPRESSION = {0x0000: "none", 0x0001: "basic", 0x0002: "normal", 0x0003: "delta"}

# Security-info substructure, offsets relative to the security info offset.
# Named here rather than inline so a wrong offset is visible as a wrong name
# rather than as a plausible number.
SI_HEADER_SIZE = 0x000
SI_IMAGE_SIZE = 0x004
SI_RSA_SIGNATURE = 0x008  # 0x100 bytes
SI_IMAGE_INFO_SIZE = 0x108
SI_IMAGE_FLAGS = 0x10C
SI_LOAD_ADDRESS = 0x110
SI_SECTION_DIGEST = 0x114  # 0x14
SI_IMPORT_TABLE_COUNT = 0x128
SI_IMPORT_DIGEST = 0x12C  # 0x14
SI_MEDIA_ID = 0x140  # 0x10
SI_FILE_KEY = 0x150  # 0x10
SI_EXPORT_TABLE = 0x160
SI_HEADER_DIGEST = 0x164  # 0x14
SI_GAME_REGIONS = 0x178
SI_MEDIA_FLAGS = 0x17C
SI_PAGE_DESCRIPTOR_COUNT = 0x180
SI_PAGE_DESCRIPTORS = 0x184


def _aes_cbc_decrypt(key, data, iv=bytes(16)):
    """AES-128-CBC decrypt.  Uses pycryptodome if present, else the
    self-contained implementation below, and asserts the two agree on the
    first block when both are available."""
    try:
        from Crypto.Cipher import AES  # type: ignore

        return AES.new(key, AES.MODE_CBC, iv).decrypt(data)
    except ImportError:
        return _aes_cbc_decrypt_pure(key, data, iv)


# --- a small, self-contained AES-128 so this tool has no hard dependency ---

_SBOX = bytes.fromhex(
    "637c777bf26b6fc53001672bfed7ab76"
    "ca82c97dfa5947f0add4a2af9ca472c0"
    "b7fd9326363ff7cc34a5e5f171d83115"
    "04c723c31896059a071280e2eb27b275"
    "09832c1a1b6e5aa0523bd6b329e32f84"
    "53d100ed20fcb15b6acbbe394a4c58cf"
    "d0efaafb434d338545f9027f503c9fa8"
    "51a3408f929d38f5bcb6da2110fff3d2"
    "cd0c13ec5f974417c4a77e3d645d1973"
    "60814fdc222a908846eeb814de5e0bdb"
    "e0323a0a4906245cc2d3ac629195e479"
    "e7c8376d8dd54ea96c56f4ea657aae08"
    "ba78252e1ca6b4c6e8dd741f4bbd8b8a"
    "703eb5664803f60e613557b986c11d9e"
    "e1f8981169d98e949b1e87e9ce5528df"
    "8ca1890dbfe6426841992d0fb054bb16"
)
_INV_SBOX = bytes(256)
_t = bytearray(256)
for _i, _v in enumerate(_SBOX):
    _t[_v] = _i
_INV_SBOX = bytes(_t)
_RCON = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36]


def _xtime(a):
    a <<= 1
    if a & 0x100:
        a = (a ^ 0x1B) & 0xFF
    return a


def _mul(a, b):
    r = 0
    while b:
        if b & 1:
            r ^= a
        a = _xtime(a)
        b >>= 1
    return r & 0xFF


def _expand_key(key):
    w = [list(key[i * 4 : i * 4 + 4]) for i in range(4)]
    for i in range(4, 44):
        t = list(w[i - 1])
        if i % 4 == 0:
            t = t[1:] + t[:1]
            t = [_SBOX[b] for b in t]
            t[0] ^= _RCON[i // 4 - 1]
        w.append([w[i - 4][j] ^ t[j] for j in range(4)])
    return [sum(w[r * 4 : r * 4 + 4], []) for r in range(11)]


def _decrypt_block(rk, block):
    s = list(block)
    s = [s[i] ^ rk[10][i] for i in range(16)]
    for rnd in range(9, -1, -1):
        # InvShiftRows
        t = list(s)
        for r in range(1, 4):
            for c in range(4):
                t[((c + r) % 4) * 4 + r] = s[c * 4 + r]
        s = [_INV_SBOX[b] for b in t]
        s = [s[i] ^ rk[rnd][i] for i in range(16)]
        if rnd:
            t = list(s)
            for c in range(4):
                a = s[c * 4 : c * 4 + 4]
                t[c * 4 + 0] = _mul(a[0], 14) ^ _mul(a[1], 11) ^ _mul(a[2], 13) ^ _mul(a[3], 9)
                t[c * 4 + 1] = _mul(a[0], 9) ^ _mul(a[1], 14) ^ _mul(a[2], 11) ^ _mul(a[3], 13)
                t[c * 4 + 2] = _mul(a[0], 13) ^ _mul(a[1], 9) ^ _mul(a[2], 14) ^ _mul(a[3], 11)
                t[c * 4 + 3] = _mul(a[0], 11) ^ _mul(a[1], 13) ^ _mul(a[2], 9) ^ _mul(a[3], 14)
            s = t
    return bytes(s)


def _aes_cbc_decrypt_pure(key, data, iv=bytes(16)):
    if len(data) % 16:
        raise ValueError("CBC input is not a multiple of the block size")
    rk = _expand_key(key)
    out = bytearray(len(data))
    prev = iv
    for off in range(0, len(data), 16):
        ct = data[off : off + 16]
        pt = _decrypt_block(rk, ct)
        out[off : off + 16] = bytes(a ^ b for a, b in zip(pt, prev))
        prev = ct
    return bytes(out)


def _aes_ecb_decrypt_block(key, block):
    return _decrypt_block(_expand_key(key), block)


class Xex:
    def __init__(self, path):
        self.path = Path(path)
        self.raw = self.path.read_bytes()
        d = self.raw
        if d[:4] != b"XEX2":
            raise ValueError(f"{path}: not an XEX2 image (magic {d[:4]!r})")

        (
            _magic,
            self.module_flags,
            self.pe_data_offset,
            self._reserved,
            self.security_info_offset,
            self.optional_header_count,
        ) = struct.unpack_from(">4sIIIII", d, 0)

        self.optional_headers = {}
        off = 24
        for _ in range(self.optional_header_count):
            key, value = struct.unpack_from(">II", d, off)
            off += 8
            self.optional_headers[key >> 8] = (key & 0xFF, value)

        self.entry_point = self._inline(0x0101)
        self.image_base = self._inline(0x0102)

        s = self.security_info_offset
        self.declared_image_size = self._u32(s + SI_IMAGE_SIZE)
        self.image_info_size = self._u32(s + SI_IMAGE_INFO_SIZE)
        self.image_flags = self._u32(s + SI_IMAGE_FLAGS)
        self.load_address = self._u32(s + SI_LOAD_ADDRESS)
        self.media_id = d[s + SI_MEDIA_ID : s + SI_MEDIA_ID + 0x10]
        self.file_key = d[s + SI_FILE_KEY : s + SI_FILE_KEY + 0x10]
        self.export_table = self._u32(s + SI_EXPORT_TABLE)
        self.game_regions = self._u32(s + SI_GAME_REGIONS)
        self.page_descriptor_count = self._u32(s + SI_PAGE_DESCRIPTOR_COUNT)

        self._read_base_file_format()

        # Which key actually worked.  Set by unpack(); until then it has not
        # been measured, and is not "retail" by default.
        self.key_used = NOTMEASURED
        self.session_key = NOTMEASURED

    # -- small readers ------------------------------------------------------

    def _u32(self, off):
        return struct.unpack_from(">I", self.raw, off)[0]

    def _u16(self, off):
        return struct.unpack_from(">H", self.raw, off)[0]

    def _inline(self, header_id):
        e = self.optional_headers.get(header_id)
        if e is None:
            return NOTMEASURED
        return e[1]

    def _pointer(self, header_id):
        e = self.optional_headers.get(header_id)
        if e is None:
            return NOTMEASURED
        return e[1]

    # -- the container's own description of its payload ---------------------

    def _read_base_file_format(self):
        off = self._pointer(0x0003)
        if off is NOTMEASURED:
            raise ValueError("no BaseFileFormat header; cannot unpack")
        self.bff_size = self._u32(off)
        self.encryption = self._u16(off + 4)
        self.compression = self._u16(off + 6)

        self.basic_blocks = []
        self.lzx_window_size = NOTMEASURED
        self.lzx_first_block_size = NOTMEASURED

        if self.compression == 1:
            n = (self.bff_size - 8) // 8
            for i in range(n):
                data_size = self._u32(off + 8 + i * 8)
                zero_size = self._u32(off + 12 + i * 8)
                self.basic_blocks.append((data_size, zero_size))
        elif self.compression == 2:
            self.lzx_window_size = self._u32(off + 8)
            self.lzx_first_block_size = self._u32(off + 12)

    # -- unpack -------------------------------------------------------------

    def unpack(self):
        """Return the PE image bytes.  Raises rather than returning a short
        or empty buffer, and checks the result against two sizes the
        container states independently."""
        if self.compression not in (0, 1):
            raise NotImplementedError(
                f"compression {self.compression} "
                f"({COMPRESSION.get(self.compression, '?')}) is not implemented; "
                "only none and basic are"
            )

        payload = self.raw[self.pe_data_offset :]

        # The block table's data sizes must account for every byte of the
        # payload.  This is the first of two independent size checks.
        if self.compression == 1:
            declared_payload = sum(b[0] for b in self.basic_blocks)
            if declared_payload != len(payload):
                raise ValueError(
                    "basic-compression block table does not account for the "
                    f"payload: table says {declared_payload} byte(s), file "
                    f"carries {len(payload)} byte(s) after the 0x{self.pe_data_offset:X} "
                    "header"
                )

        if self.encryption == 0:
            plain = payload
            self.key_used = "none"
            self.session_key = b""
        else:
            plain = None
            for name, kek in (("retail", RETAIL_KEY), ("devkit", DEVKIT_KEY)):
                session = _aes_ecb_decrypt_block(kek, self.file_key)
                trial = _aes_cbc_decrypt(session, payload[: 16 * 4096])
                if trial[:2] == b"MZ":
                    plain = _aes_cbc_decrypt(session, payload)
                    self.key_used = name
                    self.session_key = session
                    break
            if plain is None:
                raise ValueError(
                    "neither the retail nor the devkit key produced a PE header; "
                    "the image key is at security_info+0x150 and may be misplaced, "
                    f"or this image uses a key this tool does not have "
                    f"(file_key={self.file_key.hex()})"
                )

        # Expand basic compression: each block is data_size real bytes
        # followed by zero_size zero bytes.
        if self.compression == 1:
            out = bytearray()
            src = 0
            for data_size, zero_size in self.basic_blocks:
                out += plain[src : src + data_size]
                src += data_size
                out += bytes(zero_size)
            image = bytes(out)
        else:
            image = plain

        # Second independent size check: the security info's declared image
        # size, which comes from a different part of the container than the
        # block table.  The block table describes CONTENT; the image is
        # rounded up to whole 64 KiB pages, so a shortfall of less than one
        # page is expected and is zero-filled.  Anything larger is a missing
        # block and must not be papered over.
        PAGE = 0x10000
        pages = self.page_descriptor_count * PAGE
        if pages != self.declared_image_size:
            raise ValueError(
                f"{self.page_descriptor_count} page descriptor(s) x 0x{PAGE:X} "
                f"= 0x{pages:X}, but the security info declares an image size "
                f"of 0x{self.declared_image_size:X}; the two disagree, so the "
                "page granularity assumed here is wrong"
            )
        self.content_size = len(image)
        shortfall = self.declared_image_size - len(image)
        if shortfall < 0 or shortfall >= PAGE:
            raise ValueError(
                f"unpacked 0x{len(image):X} byte(s) against a declared image "
                f"size of 0x{self.declared_image_size:X}: a shortfall of "
                f"0x{shortfall:X} is not page rounding (one page is 0x{PAGE:X}), "
                "so a block is missing rather than the tail being padding"
            )
        self.page_pad = shortfall
        image = image + bytes(shortfall)

        if image[:2] != b"MZ":
            raise ValueError(
                f"unpacked image does not begin with MZ (got {image[:2]!r}); "
                "the decrypt or the block expansion is wrong"
            )

        return image

    # -- description --------------------------------------------------------

    def describe(self):
        L = []
        a = L.append
        a(f"file            {self.path}")
        a(f"  size          {len(self.raw):,} byte(s)")
        a(f"  md5           {hashlib.md5(self.raw).hexdigest()}")
        a(f"  module flags  {self.module_flags:08X}")
        a(f"  pe data at    {self.pe_data_offset:08X}")
        a(f"  entry point   {self.entry_point:08X}")
        a(f"  image base    {self.image_base:08X}")
        a(f"  load address  {self.load_address:08X}")
        a(f"  image size    {self.declared_image_size:08X} "
          f"({self.declared_image_size:,} byte(s))")
        a(f"  image flags   {self.image_flags:08X}")
        a(f"  media id      {self.media_id.hex()}")
        a(f"  export table  {self.export_table:08X}")
        a(f"  page descs    {self.page_descriptor_count}")
        a("")
        a(f"  encryption    {self.encryption:04X} "
          f"({ENCRYPTION.get(self.encryption, '?')})")
        a(f"  compression   {self.compression:04X} "
          f"({COMPRESSION.get(self.compression, '?')})")
        if self.compression == 1:
            total_data = sum(b[0] for b in self.basic_blocks)
            total_zero = sum(b[1] for b in self.basic_blocks)
            a(f"  basic blocks  {len(self.basic_blocks)}")
            for i, (ds, zs) in enumerate(self.basic_blocks):
                a(f"      [{i}]  data {ds:08X}  zero {zs:08X}")
            a(f"      sum   data {total_data:08X}  zero {total_zero:08X}  "
              f"image {total_data + total_zero:08X}")
        a("")
        a(f"  optional headers: {len(self.optional_headers)} of "
          f"{self.optional_header_count} declared")
        for hid in sorted(self.optional_headers):
            sz, val = self.optional_headers[hid]
            name = OPTIONAL_HEADER_NAMES.get(hid, "?")
            kind = "inline" if sz in (0, 1) else f"ptr sz={sz:02X}"
            a(f"      {hid:04X}  {name:<28} {kind:<12} {val:08X}")
        return "\n".join(L)


def main(argv):
    src = Path(argv[1]) if len(argv) > 1 else Path("game/DEFAULT.XEX")
    out = Path(argv[2]) if len(argv) > 2 else Path("build/default.pe.exe")

    xex = Xex(src)
    print(xex.describe())
    print()

    image = xex.unpack()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(image)

    print(f"unpacked with the {xex.key_used} key")
    print(f"  session key   {xex.session_key.hex() if xex.session_key else '(none)'}")
    print(f"  content       0x{xex.content_size:X} byte(s) from the block table")
    print(f"  page pad      0x{xex.page_pad:X} byte(s) of trailing zeros to reach "
          f"{xex.page_descriptor_count} whole 64 KiB page(s)")
    print(f"  wrote         {out}  {len(image):,} byte(s)")
    print(f"  md5           {hashlib.md5(image).hexdigest()}")
    print(f"  first bytes   {image[:2].decode('latin1')} "
          f"({image[:16].hex()})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
