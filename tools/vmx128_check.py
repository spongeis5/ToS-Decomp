"""Mark the VMX128 decoder against Biallas's independent bit tables.

`build/ppcdis.exe` decodes VMX128 using binutils' opcode table.  That table
and Biallas's `vmx128.txt` (http://biallas.net/doc/vmx128/vmx128.txt) are two
derivations with nothing in common -- one is a compiled opcode/mask list, the
other a hand-documented bit layout produced by staring at dumpbin output.
Agreement between them is evidence; agreement of a tool with itself is not.

Bit numbering here is Big-Endian IBM style, bit 0 = most significant, exactly
as the document draws it:

    lvx128    |000100| VD128 | RA | RB |0 0 0 1 1 0 0|VDh|1 1|

The register number is reassembled from its split fields, which is the part
most likely to be got wrong by hand and the part a mnemonic-only check would
not catch:

    VD128 register = (VDh << 5) | VD128
    VA128 register = (A << 6) | (a << 5) | VA128

CAVEAT ON THE ORACLE: `vmx128.txt` is known to contain errors -- Ghidra issue
#2094 records a later worker finding mistakes in it while writing a SLEIGH
implementation.  So a DISAGREEMENT here is a question, not a verdict against
binutils.  Agreement is still worth having.
"""

import re
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image
import ppcdis


def bits(w, hi, lo):
    """Bits hi..lo inclusive, IBM numbering (bit 0 = MSB of a 32-bit word)."""
    width = lo - hi + 1
    shift = 31 - lo
    return (w >> shift) & ((1 << width) - 1)


# Each entry: name, primary opcode, list of (hi, lo, expected) fixed fields,
# and a function producing the expected operand string.
def vd(w):
    return (bits(w, 28, 29) << 5) | bits(w, 6, 10)


def vb(w):
    return (bits(w, 30, 31) << 5) | bits(w, 16, 20)


def va(w):
    # A is bit 21, a is bit 26
    return (bits(w, 21, 21) << 6) | (bits(w, 26, 26) << 5) | bits(w, 11, 15)


FORMS = [
    # loads/stores: |000100| VD128 | RA | RB | xo7 |VDh|1 1|
    ("lvx128",   4, [(21, 27, 0b0001100), (30, 31, 0b11)],
     lambda w: "v%d,r%d,r%d" % (vd(w), bits(w, 11, 15), bits(w, 16, 20))),
    ("stvx128",  4, [(21, 27, 0b0011100), (30, 31, 0b11)],
     lambda w: "v%d,r%d,r%d" % (vd(w), bits(w, 11, 15), bits(w, 16, 20))),
    ("lvlx128",  4, [(21, 27, 0b1000000), (30, 31, 0b11)],
     lambda w: "v%d,r%d,r%d" % (vd(w), bits(w, 11, 15), bits(w, 16, 20))),
    ("stvlx128", 4, [(21, 27, 0b1010000), (30, 31, 0b11)],
     lambda w: "v%d,r%d,r%d" % (vd(w), bits(w, 11, 15), bits(w, 16, 20))),
    # |000101| VD | VA | VB |A|xxxx|a|1|VDh|VBh|
    ("vmulfp128", 5, [(22, 25, 0b0010), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vaddfp128", 5, [(22, 25, 0b0000), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vsubfp128", 5, [(22, 25, 0b0001), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vor128",   5, [(22, 25, 0b1011), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vand128",  5, [(22, 25, 0b1000), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vxor128",  5, [(22, 25, 0b1100), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vmsum3fp128", 5, [(22, 25, 0b0110), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    ("vmsum4fp128", 5, [(22, 25, 0b0111), (27, 27, 1)],
     lambda w: "v%d,v%d,v%d" % (vd(w), va(w), vb(w))),
    # |000110| VD | UIMM | VB |1 1 1 0 0 1 1|VDh|VBh|
    ("vspltw128", 6, [(21, 27, 0b1110011)],
     lambda w: "v%d,v%d,%d" % (vd(w), vb(w), bits(w, 11, 15))),
    ("vupkd3d128", 6, [(21, 27, 0b1111111)],
     lambda w: "v%d,v%d,%d" % (vd(w), vb(w), bits(w, 11, 15))),
    ("vrsqrtefp128", 6, [(21, 27, 0b1100111), (11, 15, 0)],
     lambda w: "v%d,v%d" % (vd(w), vb(w))),
    ("vrefp128", 6, [(21, 27, 0b1100011), (11, 15, 0)],
     lambda w: "v%d,v%d" % (vd(w), vb(w))),
]


def match_form(w):
    op = bits(w, 0, 5)
    for name, prim, fixed, ops in FORMS:
        if op != prim:
            continue
        if all(bits(w, hi, lo) == val for hi, lo, val in fixed):
            return name, ops(w)
    return None


def main():
    img = Image()
    if not ppcdis.available():
        print("build/ppcdis.exe missing", file=sys.stderr)
        return 1

    # Collect every distinct opcode-4/5/6 word in .text, with one address each.
    seen = {}
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        off = s["va"] - img.base
        n = (s["vsize"] or s["rawsz"]) // 4
        words = struct.unpack_from(">%dI" % n, img.data, off)
        for i, w in enumerate(words):
            if (w >> 26) in (4, 5, 6) and w not in seen:
                seen[w] = s["va"] + i * 4
    print("%d distinct opcode-4/5/6 word(s) in .text" % len(seen))

    st = Counter()
    disagree = []
    items = sorted(seen.items(), key=lambda kv: kv[1])
    # Disassemble each in one batch per address (they are scattered).
    for w, va_ in items:
        doc = match_form(w)
        if doc is None:
            st["no_doc_form"] += 1
            continue
        name, ops = doc
        got = ppcdis.words([w], va_)[0][2]
        parts = got.split(None, 1)
        gm = parts[0]
        go = parts[1].replace(" ", "") if len(parts) > 1 else ""
        st["checked"] += 1
        if gm == name and go == ops:
            st["agree"] += 1
        elif gm == name:
            st["mnemonic_only"] += 1
            if len(disagree) < 12:
                disagree.append((va_, w, name, ops, got, "operands"))
        else:
            st["mnemonic_differs"] += 1
            if len(disagree) < 12:
                disagree.append((va_, w, name, ops, got, "mnemonic"))

    print()
    print("forms covered by the transcribed tables : %d word(s)" % st["checked"])
    print("  not one of the 16 forms transcribed   : %d" % st["no_doc_form"])
    print()
    print("  AGREE on mnemonic AND operands        : %d" % st["agree"])
    print("  agree on mnemonic, differ on operands : %d" % st["mnemonic_only"])
    print("  differ on mnemonic                    : %d" % st["mnemonic_differs"])

    if disagree:
        print("\n  disagreements:")
        for va_, w, name, ops, got, kind in disagree:
            print("    %08X %08X  doc: %-12s %-22s binutils: %s   (%s)"
                  % (va_, w, name, ops, got, kind))
    else:
        print("\n  No disagreement on any checked word. Two independent")
        print("  derivations of the VMX128 encoding produce the same")
        print("  mnemonic AND the same reassembled 7-bit register numbers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
