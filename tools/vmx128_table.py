"""Exhaustive VMX128 table check: binutils vs the documentation vs MSVC.

Earlier checks covered 16 hand-transcribed forms and 33 forms the compiler
happened to emit.  That left forms verified by nothing, and it missed a real
error: `vmx128.txt` gives `vandc128` and `vnor128` the SAME encoding
(`|A|1 0 1 0|a|1|`), which cannot both be right.

This does three things over the WHOLE table:

 1. Parse every VX128 entry out of binutils' `ppc-dis.c` -- name, opcode
    value, mask -- rather than transcribing them.
 2. Check the table against itself over the real image: every VMX128 word must
    match EXACTLY ONE entry.  Two matches is an ambiguity, zero is a hole.
 3. Diff binutils' extended-opcode bits against the documentation for every
    form the document describes, and report every disagreement rather than the
    one that happened to be noticed.

Disagreements are then resolvable against MSVC, which is the only
non-reconstructed source of the three.
"""

import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image

PPCDIS = Path("thirdparty/disasm/ppc-dis.c")

# The macro family, read from ppc-dis.c lines 1810..1858.
#   OP(x)            = (x & 0x3f) << 26
#   VX(op, xop)      = OP(op) | (xop & 0x7ff)
#   VX128(op, xop)   = OP(op) | (xop & 0x3d0)
#   VX128_1(op,xop)  = OP(op) | (xop & 0x7f3)
#   VX128_2(op,xop)  = OP(op) | (xop & 0x210)
#   VX128_3(op,xop)  = OP(op) | (xop & 0x7f0)
#   VX128_4(op,xop)  = OP(op) | (xop & 0x730)
#   VX128_5(op,xop)  = OP(op) | (xop & 0x10)
#   VX128_P(op,xop)  = OP(op) | (xop & 0x630)
# and each _MASK is VX(0x3f, <same constant>).
FORM_MASK = {
    "VX128":   0x3D0,
    "VX128_1": 0x7F3,
    "VX128_2": 0x210,
    "VX128_3": 0x7F0,
    "VX128_4": 0x730,
    "VX128_5": 0x010,
    "VX128_P": 0x630,
}

ENTRY = re.compile(
    r'\{\s*"([a-z0-9_.]+)"\s*,\s*(VX128(?:_[0-9P])?)\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)')


def parse_binutils():
    """Entries from ppc-dis.c, SKIPPING COMMENTED-OUT LINES.

    A first version matched the regex against the whole file and picked up
    `//{ "vupkhsh128", ... }` and `//{ "vupklsh128", ... }`, which are disabled
    in the source.  That produced 348 words "matching more than one entry" and
    an ambiguity report that was entirely an artefact of the extractor.
    """
    out = []
    for raw in PPCDIS.read_text(errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("//") or line.startswith("/*"):
            continue
        m = ENTRY.search(line)
        if not m:
            continue
        name, form, op, xop = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        const = FORM_MASK[form]
        opcode = ((op & 0x3F) << 26) | (xop & const)
        mask = ((0x3F) << 26) | const
        out.append(dict(name=name, form=form, op=op, xop=xop,
                        opcode=opcode, mask=mask))
    return out


# The documentation's 4-bit extended field for opcode-5 and opcode-6 forms,
# transcribed from vmx128.txt.  IBM bit numbering: the field is bits 22..25,
# with bit 21 = A, bit 26 = a, bit 27 the form selector.
DOC_OP5 = {   # bit27 == 1
    "vaddfp128": 0b0000, "vsubfp128": 0b0001, "vmulfp128": 0b0010,
    "vmaddfp128": 0b0011, "vmaddcfp128": 0b0100, "vnmsubfp128": 0b0101,
    "vmsum3fp128": 0b0110, "vmsum4fp128": 0b0111,
    "vand128": 0b1000, "vandc128": 0b1010, "vnor128": 0b1010,
    "vor128": 0b1011, "vxor128": 0b1100, "vsel128": 0b1101,
    "vslo128": 0b1110, "vrlw128": 0b0001,
}
DOC_OP5_BIT27_0 = {   # bit27 == 0  (the pack family)
    "vpkshss128": 0b1000, "vpkshus128": 0b1001, "vpkswss128": 0b1010,
    "vpkswus128": 0b1011, "vpkuhum128": 0b1100, "vpkuhus128": 0b1101,
    "vpkuwum128": 0b1110, "vpkuwus128": 0b1111,
}
DOC_OP6 = {   # bit27 == 0 unless noted
    "vmaxfp128": 0b1010, "vminfp128": 0b1011,
    "vmrghw128": 0b1100, "vmrglw128": 0b1101,
    "vslw128": 0b0011, "vsraw128": 0b0101, "vsrw128": 0b0111,
    "vsro128": 0b1111,
}


def bits(w, hi, lo):
    return (w >> (31 - lo)) & ((1 << (lo - hi + 1)) - 1)


def main():
    entries = parse_binutils()
    print("VX128 entries parsed from binutils ppc-dis.c: %d" % len(entries))
    byform = Counter(e["form"] for e in entries)
    print("  by macro form: %s" % ", ".join("%s %d" % kv for kv in sorted(byform.items())))

    # ---- 1. table self-consistency over the real image ----
    img = Image()
    words = set()
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        off = s["va"] - img.base
        n = (s["vsize"] or s["rawsz"]) // 4
        for w in struct.unpack_from(">%dI" % n, img.data, off):
            if (w >> 26) in (4, 5, 6):
                words.add(w)
    print("\ndistinct opcode-4/5/6 word(s) in .text: %d" % len(words))

    hits = Counter()
    ambiguous = []
    for w in words:
        m = [e for e in entries if (w & e["mask"]) == e["opcode"]]
        hits[len(m)] += 1
        if len(m) > 1 and len(ambiguous) < 8:
            ambiguous.append((w, [e["name"] for e in m]))
    print("  matching exactly one VX128 entry : %d" % hits[1])
    print("  matching NONE (plain VMX/Altivec): %d" % hits[0])
    print("  matching MORE THAN ONE           : %d" % sum(v for k, v in hits.items() if k > 1))
    if ambiguous:
        print("\n  AMBIGUOUS WORDS -- the table cannot decide these:")
        for w, names in ambiguous:
            print("     %08X -> %s" % (w, ", ".join(names)))
    else:
        print("  no word matches two entries: the table is unambiguous here")

    # ---- 2. binutils vs the documentation, every form the doc describes ----
    print("\nbinutils extended-opcode bits vs vmx128.txt:")
    byname = {e["name"]: e for e in entries}
    disagree = []
    checked = 0
    for table, want27 in ((DOC_OP5, 1), (DOC_OP5_BIT27_0, 0), (DOC_OP6, None)):
        for name, doc_xo in table.items():
            e = byname.get(name)
            if e is None:
                print("   %-14s NOT IN binutils" % name)
                continue
            checked += 1
            got_xo = bits(e["opcode"], 22, 25)
            got27 = bits(e["opcode"], 27, 27)
            ok = (got_xo == doc_xo) and (want27 is None or got27 == want27)
            if not ok:
                disagree.append((name, doc_xo, got_xo, want27, got27))
    print("   forms compared: %d" % checked)
    if disagree:
        print("   DISAGREEMENTS:")
        for name, d, g, w27, g27 in disagree:
            print("     %-14s doc xo=%s bit27=%s   binutils xo=%s bit27=%d"
                  % (name, format(d, "04b"), w27 if w27 is not None else "-",
                     format(g, "04b"), g27))
    else:
        print("   no disagreement")

    # ---- 3. the specific collision ----
    print("\nthe documented collision:")
    for n in ("vandc128", "vnor128", "vand128", "vor128"):
        e = byname[n]
        print("   %-10s binutils xo=%s bit27=%d   (VX128(%d,%d))"
              % (n, format(bits(e["opcode"], 22, 25), "04b"),
                 bits(e["opcode"], 27, 27), e["op"], e["xop"]))
    print("   vmx128.txt gives vandc128 and vnor128 BOTH as 1010, which cannot")
    print("   both be right. binutils separates them as 1001 / 1010.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
