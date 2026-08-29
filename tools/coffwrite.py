"""Write the two COFF objects a real link needs that no compiler produces.

A decompilation cannot link until it can answer two questions the source does
not contain:

  1. **Where is a function we have not written yet?** `BinAlloc` calls
     `RefillBin`, which has no source here. `link.exe` will not resolve it,
     and `/FORCE:UNRESOLVED` would let it invent an address -- which is the
     one thing that must not happen, because the bytes would then verify
     against a number the linker chose rather than one the image states.
     `absolutes()` writes an object defining each such name as an ABSOLUTE
     COFF symbol at the address READ OUT OF THE RETAIL IMAGE. The linker then
     resolves the call for real, and the displacement it computes is checkable.

  2. **How does our code land at the address the image puts it at?** The
     linker chooses section RVAs; we need a specific one. `padding()` writes
     an object holding a single `.text` block of exactly N bytes, ordered
     first, which shifts everything after it by N.

Both are COFF objects with no compiler involved, so both are written here
rather than fabricated as assembly and passed through a tool that would
reformat them.

The formats are IMAGE_FILE_HEADER and IMAGE_SYMBOL from winnt.h. Nothing
about them is Xbox-specific except the machine id.
"""

import struct

IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2

IMAGE_SYM_UNDEFINED = 0
IMAGE_SYM_ABSOLUTE = -1

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3

IMAGE_SYM_DTYPE_FUNCTION = 0x20

IMAGE_SCN_CNT_CODE = 0x00000020
IMAGE_SCN_LNK_COMDAT = 0x00001000
IMAGE_SCN_ALIGN_4BYTES = 0x00300000
# cl.exe marks every function COMDAT it emits with this, which is why a
# function ending at 4 mod 8 is followed by four bytes of nothing.
IMAGE_SCN_ALIGN_8BYTES = 0x00400000
IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_SCN_MEM_READ = 0x40000000

IMAGE_COMDAT_SELECT_NODUPLICATES = 1


class _Strings(object):
    """The COFF string table: names longer than 8 bytes live here.

    Every mangled C++ name is longer than 8 bytes, so in practice every
    symbol this module writes is a string-table reference.
    """

    def __init__(self):
        self.blob = bytearray(b"\0\0\0\0")      # length field, filled at end
        self.at = {}

    def ref(self, name):
        """-> the 8-byte name field for `name`."""
        raw = name.encode("latin1")
        if len(raw) <= 8:
            return raw + bytes(8 - len(raw))
        if name not in self.at:
            self.at[name] = len(self.blob)
            self.blob += raw + b"\0"
        return struct.pack("<II", 0, self.at[name])

    def bytes(self):
        out = bytearray(self.blob)
        struct.pack_into("<I", out, 0, len(out))
        return bytes(out)


def _symbol(namefield, value, secnum, dtype, cls):
    return struct.pack("<8sIhHBB", namefield, value, secnum, dtype, cls, 0)


def absolutes(symbols):
    """An object defining each `name -> address` as an ABSOLUTE symbol.

    `symbols` is a mapping of mangled name to guest address. The linker will
    resolve a REL24 against one of these by computing `address - site`, and a
    REFHI/REFLO pair by splitting `address` -- exactly what it does for a real
    definition, which is the point: the arithmetic is the linker's, not ours,
    so a wrong one shows up as a wrong instruction rather than being copied
    in from the image.
    """
    st = _Strings()
    table = bytearray()
    for name in sorted(symbols):
        table += _symbol(st.ref(name), symbols[name] & 0xFFFFFFFF,
                         IMAGE_SYM_ABSOLUTE, IMAGE_SYM_DTYPE_FUNCTION,
                         IMAGE_SYM_CLASS_EXTERNAL)
    nsym = len(symbols)
    psym = 20 if nsym else 0
    head = struct.pack("<HHIIIHH", IMAGE_FILE_MACHINE_POWERPCBE, 0, 0,
                       psym, nsym, 0, 0)
    return head + bytes(table) + st.bytes()


def padding(nbytes, fill=b"\0", name="__tos_pad"):
    """An object whose `.text` is exactly `nbytes`, under one COMDAT symbol.

    Ordered first by `/ORDER`, this is how a link is made to place the code
    that follows it at a chosen address.

    It has to be a COMDAT, and that is MEASURED: `/ORDER` orders COMDATs and
    nothing else, so an ordinary `.text` contribution is placed wherever the
    linker likes. Named first in the order file as a plain section, the pad
    landed AFTER all 55 ordered functions -- at +0x36c, exactly past them --
    and the run it was supposed to push forward started at offset zero
    instead.

    `fill` is repeated to length. Zero is what the retail linker leaves
    between functions (298 of 298 measured gaps), but a caller reproducing a
    span of the image passes the image's own bytes.

    The linker will not emit a section of zero length, so a zero request
    returns an object with no section and no symbol -- the caller must not
    then name it in the order file.
    """
    if nbytes < 0:
        raise ValueError("negative padding: %d" % nbytes)
    if nbytes == 0:
        return absolutes({})
    if nbytes % 4:
        raise ValueError("padding must be a multiple of 4: %d" % nbytes)
    if not fill:
        raise ValueError("empty fill")

    st = _Strings()
    data = (fill * (nbytes // len(fill) + 1))[:nbytes]
    secoff = 20 + 40
    psym = secoff + len(data)
    chars = (IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_8BYTES | IMAGE_SCN_LNK_COMDAT
             | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ)
    sec = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(data),
                      secoff, 0, 0, 0, 0, chars)

    # The COMDAT trio, in the order the format requires: the STATIC symbol
    # whose name is the section's, its section-definition aux record carrying
    # the selection, and only then the external symbol that names the COMDAT.
    # This is the same shape cl.exe emits for a function COMDAT -- `.text`
    # static + aux with selection 1, then the mangled name.
    syms = _symbol(b".text\0\0\0", 0, 1, 0, IMAGE_SYM_CLASS_STATIC)
    syms = syms[:17] + bytes([1])                    # NumberOfAuxSymbols = 1
    syms += struct.pack("<IHHIHB3s", len(data), 0, 0, 0, 0,
                        IMAGE_COMDAT_SELECT_NODUPLICATES, bytes(3))
    syms += _symbol(st.ref(name), 0, 1, IMAGE_SYM_DTYPE_FUNCTION,
                    IMAGE_SYM_CLASS_EXTERNAL)
    head = struct.pack("<HHIIIHH", IMAGE_FILE_MACHINE_POWERPCBE, 1, 0,
                       psym, 3, 0, 0)
    return head + sec + data + syms + st.bytes()
