"""Rewrite the unpacked image's section headers so PointerToRawData equals
VirtualAddress.

The unpacked XEX is a memory image -- section data already sits at its RVA in
the buffer -- but the PE headers carry the ORIGINAL file layout in
PointerToRawData.  Ghidra's PE loader believes those headers, so it maps the
wrong bytes for every section from .text onward.

This makes the headers describe the file as it actually is.  Nothing is moved;
only the two fields that lie are corrected.

Verified after writing: the entry point must disassemble as a function
prologue, and every section's first bytes must be identical to the same VA
read through the RVA mapping of the original.
"""

import struct
import sys
from pathlib import Path

SRC = Path("build/default.pe.exe")
DST = Path("build/default.image.exe")


def main():
    data = bytearray(SRC.read_bytes())
    o = struct.unpack_from("<I", data, 0x3C)[0]
    if bytes(data[o : o + 4]) != b"PE\0\0":
        raise SystemExit("not a PE image")
    nsec = struct.unpack_from("<H", data, o + 6)[0]
    optsz = struct.unpack_from("<H", data, o + 20)[0]
    base = struct.unpack_from("<I", data, o + 24 + 28)[0]
    entry_rva = struct.unpack_from("<I", data, o + 24 + 16)[0]
    sh = o + 24 + optsz

    print("image base %08X  entry rva %08X (va %08X)  %d section(s)"
          % (base, entry_rva, base + entry_rva, nsec))
    changed = 0
    for i in range(nsec):
        b = sh + i * 40
        name = bytes(data[b : b + 8]).rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, b + 8)
        # SizeOfRawData must cover VirtualSize, and must not run past the file.
        newsz = max(rawsz, vsize)
        if va + newsz > len(data):
            newsz = len(data) - va
        if rawptr != va or rawsz != newsz:
            struct.pack_into("<I", data, b + 16, newsz)   # SizeOfRawData
            struct.pack_into("<I", data, b + 20, va)      # PointerToRawData
            changed += 1
            print("  %-10s rawptr %08X -> %08X   rawsz %08X -> %08X"
                  % (name, rawptr, va, rawsz, newsz))
        else:
            print("  %-10s already correct (%08X)" % (name, va))

    DST.write_bytes(bytes(data))
    print("\nrewrote %d of %d section header(s); wrote %s (%d bytes)"
          % (changed, nsec, DST, len(data)))

    # --- verification, against the ORIGINAL read through the RVA mapping ---
    src = SRC.read_bytes()
    dst = DST.read_bytes()
    if len(src) != len(dst):
        raise SystemExit("length changed -- refusing")
    body_diff = sum(1 for i in range(len(src))
                    if src[i] != dst[i] and not (sh <= i < sh + nsec * 40))
    print("bytes differing outside the section table: %d (must be 0)" % body_diff)
    if body_diff:
        raise SystemExit("content changed -- refusing")

    try:
        from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
    except ImportError:
        print("capstone absent; skipping the prologue check")
        return 0
    md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    ins = list(md.disasm(dst[entry_rva : entry_rva + 16], base + entry_rva))
    print("\nentry point disassembly, via the corrected mapping:")
    for i in ins[:4]:
        print("  %08X  %-8s %s" % (i.address, i.mnemonic, i.op_str))
    if not ins or ins[0].mnemonic not in ("mflr", "stwu", "mfspr", "li", "addi"):
        raise SystemExit("entry point does not begin like a function -- "
                         "the mapping is still wrong")
    print("\nentry point begins with a function prologue: mapping confirmed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
