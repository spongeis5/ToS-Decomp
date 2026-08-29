"""Does adjacency predict the optimisation level? Measure it over every match.

    python tools/flagpairs.py

MATCHED.md's claim that the level is a property of the TRANSLATION UNIT rests
on six adjacent pairs that agreed. Six is not many, and by now there are far
more matches to ask with. This compiles every matched function at BOTH
levels, classifies it, and reports every adjacent pair.

The classification is the point, and it is three-way, not two:

    /O2 only     matches at /O2 and not at /O2 /Os
    /Os only     matches at /O2 /Os and not at /O2
    insensitive  matches at both -- carries NO evidence either way

Insensitive functions must be excluded from the pair count. Counting them as
agreements is how a claim like this gets inflated: most small accessors are
insensitive, so a table that includes them would report near-total agreement
no matter what the truth was. Only a pair where BOTH sides are sensitive can
confirm or refute anything.

A pair that DISAGREES is the interesting result, because it would mean either
that the level is not per-unit or that the two functions are in different
units despite being adjacent. Any such pair is printed in full.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image
from libmatch import coff_functions, trim_padding
from match import can_shrink, can_extend

import xdkcc

ROOT = Path(__file__).resolve().parent.parent
O2 = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]
OS_ = ["/c", "/nologo", "/O2", "/Os", "/Gy", "/GS-", "/fp:fast"]
GAP = 0x400          # "adjacent" -- generous, and reported per pair anyway


def manifest():
    out = []
    for line in (ROOT / "src/manifest.txt").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split()
        sym = None
        for extra in f[2:]:
            if not extra.startswith("flags=") and extra != "-":
                sym = extra
        out.append((f[0], int(f[1], 16), sym))
    return out


def matches_at(img, src, addr, sym, flags, work):
    tag = Path(src).stem + ("_" + sym if sym else "") + \
        ("_os" if "/Os" in flags else "_o2")
    blob, _err = xdkcc.compile_obj(str(ROOT / src), work / (tag + ".obj"),
                                   flags, work)
    if blob is None:
        return None
    fns = coff_functions(blob)
    if sym:
        picked = [f for f in fns if ("?" + sym + "@@") in f[0]] \
            or [f for f in fns if sym in f[0]]
        fns = picked or fns
    if not fns:
        return None
    _n, code, mask = max(fns, key=lambda f: len(f[1]))
    code, mask = trim_padding(code, mask)
    from peimage import load_inventory
    sizes = dict(load_inventory())
    tsize = sizes.get(addr)
    if tsize is None:
        return None
    tbytes = img.read(addr, tsize)
    if tbytes is None:
        return None
    # BOTH reconciliations, exactly as match.py applies them. With only the
    # shrink, every function whose recorded size is too SHORT -- a switch
    # with its jump table inline, a row truncated by a false start -- reads
    # as "matches at neither level", which is a tool disagreeing with
    # verify.py rather than a fact about the function. Three did.
    grown = can_extend(img, sizes, code, mask, addr, tsize)
    if grown is not None:
        tbytes, tsize = grown, len(code)
    elif can_shrink(code, mask, tbytes, addr, tsize):
        tbytes, tsize = tbytes[:len(code)], len(code)
    if len(code) != tsize:
        return False
    for i in range(len(code) // 4):
        if not all(mask[i * 4:i * 4 + 4]):
            continue
        if (struct.unpack_from(">I", tbytes, i * 4)[0]
                != struct.unpack_from(">I", code, i * 4)[0]):
            return False
    return True


def main():
    img = Image()
    work = ROOT / "build/flagpairs"
    work.mkdir(parents=True, exist_ok=True)

    rows = []
    for src, addr, sym in manifest():
        a = matches_at(img, src, addr, sym, O2, work)
        b = matches_at(img, src, addr, sym, OS_, work)
        if a is None or b is None:
            print("  %-28s %08X  WOULD NOT COMPILE -- excluded"
                  % (Path(src).name, addr))
            continue
        kind = ("insensitive" if (a and b) else
                "/O2 only" if a else
                "/Os only" if b else "NEITHER")
        rows.append((addr, Path(src).name, kind))
    rows.sort()

    broken = [r for r in rows if r[2] == "NEITHER"]
    sens = [r for r in rows if r[2] in ("/O2 only", "/Os only")]
    print("")
    print("%d matched function(s) classified" % len(rows))
    print("  /O2 only     %d" % sum(1 for r in rows if r[2] == "/O2 only"))
    print("  /Os only     %d" % sum(1 for r in rows if r[2] == "/Os only"))
    print("  insensitive  %d   <- carries no evidence, excluded from pairs"
          % sum(1 for r in rows if r[2] == "insensitive"))
    if broken:
        print("  MATCHES AT NEITHER  %d -- these should not be in the"
              % len(broken))
        print("  manifest at all:")
        for a, s, _k in broken:
            print("      %08X  %s" % (a, s))

    print("")
    print("ADJACENT PAIRS (gap <= %d bytes) where BOTH sides are sensitive"
          % GAP)
    print("  %-10s %-10s %-6s %-12s %-12s %s"
          % ("first", "second", "gap", "first", "second", "verdict"))
    agree = disagree = 0
    for i in range(len(sens) - 1):
        a1, s1, k1 = sens[i]
        a2, s2, k2 = sens[i + 1]
        gap = a2 - a1
        if gap > GAP:
            continue
        ok = (k1 == k2)
        agree += ok
        disagree += (not ok)
        print("  %08X   %08X   %-6d %-12s %-12s %s"
              % (a1, a2, gap, k1, k2, "AGREE" if ok else "*** DISAGREE ***"))
    print("")
    print("  %d agreement(s), %d disagreement(s), out of %d informative pair(s)"
          % (agree, disagree, agree + disagree))
    if disagree:
        print("")
        print("  A DISAGREEING PAIR means either the level is not a per-unit")
        print("  property or those two functions are in different units")
        print("  despite being adjacent. Either way the claim in MATCHED.md")
        print("  needs rewriting, not defending.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
