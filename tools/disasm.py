"""Disassemble a range of the retail image by guest address.

    python tools/dis.py 82908B60 60          60 instructions from there
    python tools/dis.py 82908BA0 --fn        the whole .pdata function
    python tools/dis.py 82908BA0 40 --back 20

Capstone does not know Xenon's VMX128 extension, so a word it cannot decode
is printed as a raw word with a marker rather than skipped.  A disassembly
that silently drops instructions is a disassembly of a different function.

Where a `lis`/`addi` pair forms an address, the resolved value is annotated,
and if it lands on a printable string that string is shown -- which is what
makes a reference readable without a second tool.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions

import ppcdis


def string_at(img, va, limit=90):
    b = img.read(va, limit)
    if not b:
        return None
    out = []
    for c in b:
        if c == 0:
            break
        if not (0x20 <= c < 0x7F):
            return None
        out.append(chr(c))
    if len(out) < 4:
        return None
    return "".join(out)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    img = Image()
    funcs = load_functions()
    start = int(argv[1], 16)

    back = 0
    if "--back" in argv:
        back = int(argv[argv.index("--back") + 1])

    if "--fn" in argv:
        owner = None
        for a, s in funcs:
            if a <= start < a + s:
                owner = (a, s)
                break
        if owner is None:
            print("%08X is not inside any .pdata function; "
                  "give an instruction count instead" % start)
            return 1
        start, count = owner[0], owner[1] // 4
        print("; .pdata function sub_%08X, %d bytes" % (owner[0], owner[1]))
    else:
        count = 40
        for a in argv[2:]:
            if not a.startswith("--") and a.isdigit():
                count = int(a)
                break

    start -= back * 4
    count += back

    fstarts = {a for a, _ in funcs}
    pending = {}          # register -> (hi, address of the lis)
    lines = {va: (w, t) for va, w, t in ppcdis.image_range(start, count)}

    for i in range(count):
        va = start + i * 4
        raw = img.read(va, 4)
        if raw is None:
            print("%08X  <not backed>" % va)
            continue
        w = struct.unpack(">I", raw)[0]
        mark = " <=" if va == start + back * 4 else "   "
        label = "  sub_%08X:" % va if va in fstarts else ""
        if label:
            print(label)

        note = ""
        op = w >> 26
        if op == 15 and ((w >> 16) & 0x1F) == 0:          # lis rD, hi
            pending[(w >> 21) & 0x1F] = w & 0xFFFF
        elif op == 14:                                     # addi rD, rA, SIMM
            a = (w >> 16) & 0x1F
            if a in pending:
                lo = w & 0xFFFF
                if lo >= 0x8000:
                    lo -= 0x10000
                val = ((pending[a] << 16) + lo) & 0xFFFFFFFF
                s = string_at(img, val)
                note = "   ; = %08X%s" % (val, ('  "%s"' % s) if s else "")
        elif op == 24:                                     # ori rA, rS, UIMM
            srcr = (w >> 21) & 0x1F
            if srcr in pending:
                val = ((pending[srcr] << 16) | (w & 0xFFFF)) & 0xFFFFFFFF
                s = string_at(img, val)
                note = "   ; = %08X%s" % (val, ('  "%s"' % s) if s else "")

        text = lines.get(va, (w, "<not disassembled>"))[1]
        print("%08X%s %08x  %-40s%s" % (va, mark, w, text, note))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
