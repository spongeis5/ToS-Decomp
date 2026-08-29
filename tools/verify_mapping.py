"""Decide the image's VA -> offset mapping by a SEMANTIC control.

The previous check compared Ghidra's bytes against this project's own read of
the file -- and both used PointerToRawData, so they agreed while both were
wrong.  Two derivations that share a defect are one derivation run twice.

This does not compare two readers.  It asks a question only the CORRECT
mapping can answer well, using an oracle that has no part in the mapping:
`.pdata` states each function's exact extent, so under a correct mapping

  * the FIRST word should be a plausible prologue, and
  * the LAST word should be a terminator -- blr, bctr, or an unconditional b.

Random bytes cannot do this.  Both mappings are scored so the answer is a
comparison and not an assertion, and the losing arm is printed rather than
hidden -- a check with only one arm cannot be shown to discriminate.
"""

import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import load_functions

IMAGE = Path("build/default.pe.exe")
BASE = 0x82000000

BLR = 0x4E800020
BCTR = 0x4E800420
BCTRL = 0x4E800421


def sections(data):
    o = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, o + 6)[0]
    optsz = struct.unpack_from("<H", data, o + 20)[0]
    sh = o + 24 + optsz
    out = []
    for i in range(nsec):
        b = sh + i * 40
        name = data[b : b + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, b + 8)
        out.append(dict(name=name, va=BASE + va, vsize=vsize,
                        rawsz=rawsz, rawptr=rawptr))
    return out


def off_rawptr(secs, va):
    for s in secs:
        if s["rawptr"] and s["va"] <= va < s["va"] + min(s["vsize"] or s["rawsz"], s["rawsz"]):
            return s["rawptr"] + (va - s["va"])
    return None


def off_rva(_secs, va):
    return va - BASE


def is_terminator(w):
    if w in (BLR, BCTR, BCTRL):
        return True
    op = w >> 26
    if op == 18:                       # b / ba / bl / bla
        return (w & 1) == 0            # not a call
    if op == 19:                       # bclr / bcctr family
        xo = (w >> 1) & 0x3FF
        return xo in (16, 528)
    return False


def is_prologue(w):
    op = w >> 26
    if op == 31 and ((w >> 1) & 0x3FF) == 339:      # mfspr (mflr)
        return True
    if op == 37 and ((w >> 16) & 0x1F) == 1:        # stwu rX, d(r1)
        return True
    if op in (14, 15, 24, 32, 36, 37, 38, 44, 46, 47, 18, 19, 31, 34, 40, 48, 52):
        return True
    return False


def score(data, secs, off_fn, funcs):
    st = Counter()
    for addr, size in funcs:
        a = off_fn(secs, addr)
        if a is None or a + size > len(data) or size < 8:
            st["unmapped"] += 1
            continue
        st["checked"] += 1
        first = struct.unpack_from(">I", data, a)[0]
        last = struct.unpack_from(">I", data, a + size - 4)[0]
        if first == 0:
            st["first_is_zero"] += 1
        if is_prologue(first):
            st["prologue"] += 1
        if is_terminator(last):
            st["terminator"] += 1
        if is_prologue(first) and is_terminator(last):
            st["both"] += 1
    return st


def main():
    data = IMAGE.read_bytes()
    secs = sections(data)
    funcs = load_functions()
    print("oracle: %d function extents from .pdata, which has no part in "
          "either mapping\n" % len(funcs))

    rows = []
    for label, fn in (("PointerToRawData", off_rawptr), ("RVA == offset", off_rva)):
        st = score(data, secs, fn, funcs)
        c = st["checked"] or 1
        rows.append((label, st, c))
        print("  %-18s checked %6d  unmapped %5d" % (label, st["checked"], st["unmapped"]))
        print("      first word is a prologue  %6d  (%5.1f%%)"
              % (st["prologue"], 100.0 * st["prologue"] / c))
        print("      last word is a terminator %6d  (%5.1f%%)"
              % (st["terminator"], 100.0 * st["terminator"] / c))
        print("      BOTH                      %6d  (%5.1f%%)"
              % (st["both"], 100.0 * st["both"] / c))
        print("      first word is zero        %6d" % st["first_is_zero"])
        print()

    (la, sa, ca), (lb, sb, cb) = rows
    pa = 100.0 * sa["both"] / ca
    pb = 100.0 * sb["both"] / cb
    print("VERDICT")
    if pb > pa * 1.5 and pb > 60.0:
        print("  %s wins decisively: %.1f%% against %.1f%%." % (lb, pb, pa))
        print("  The terminator test is the load-bearing half -- a wrong")
        print("  mapping still yields decodable instructions, but it cannot")
        print("  place a return at the exact byte .pdata says the function ends.")
        return 0
    if pa > pb * 1.5 and pa > 60.0:
        print("  %s wins: %.1f%% against %.1f%%." % (la, pa, pb))
        return 0
    print("  NEITHER mapping is convincing (%.1f%% vs %.1f%%). This is "
          "NOT_MEASURED, not a result." % (pa, pb))
    return 2


if __name__ == "__main__":
    sys.exit(main())
