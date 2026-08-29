"""Which unmatched functions would tell us the most about a TYPE?

    python tools/layout.py                 the best structural targets
    python tools/layout.py 826A3328        what one function reveals
    python tools/layout.py --shared        types already touched by matches

Counting matched functions measures throughput. It does not measure whether
the project understands the program, and README gap 2 is explicit that it
does not: "only one type identity across files is actually supported by
evidence, so most structs are still per-file."

A function is structurally valuable when matching it PINS LAYOUT:

  offsets    how many distinct field offsets it touches on its first
             argument -- a constructor that writes twelve fields fixes
             twelve ASSERT_OFFSETs at once
  span       the largest offset it reaches, which is a lower bound on
             sizeof(T) -- and a 0x8D8 field says far more than a 0x10 one
  vtable     whether it stores a lis/addi-formed address at +0, which
             identifies the CLASS and not merely a struct
  stride     any `mulli` immediate, which is an element size EXACTLY, and
             the only thing that justifies an ASSERT_SIZE

The last column is the one that attacks gap 2 directly: how many functions
this project has ALREADY matched touch the same global or vtable. A type
reached from two matched files is a type identity supported by evidence
rather than assumed, which is the bar `src/owner_clear.cpp` had to clear.
"""

import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent

# D-form loads and stores, opcode -> width. Everything indexed off a base
# register; we only care about ones based on r3, the first argument.
DFORM = {32: "lwz", 33: "lwzu", 34: "lbz", 36: "stw", 37: "stwu",
         38: "stb", 40: "lhz", 42: "lha", 44: "sth", 48: "lfs",
         52: "stfs", 50: "lfd", 54: "stfd"}
STORES = (36, 37, 38, 44, 52, 54)


def analyse(img, va, size):
    """-> (offsets, stores, span, vtable_at_zero, strides, globals)"""
    n = size // 4
    data = img.read(va, size)
    if data is None or len(data) != size:
        return None
    words = struct.unpack(">%dI" % n, data)
    offsets, stores, strides, globs = set(), set(), set(), set()
    pending = {}
    vtable = False
    for i, w in enumerate(words):
        op = w >> 26
        if op == 15 and ((w >> 16) & 0x1F) == 0:
            pending[(w >> 21) & 0x1F] = w & 0xFFFF
            continue
        if op == 14:
            ra = (w >> 16) & 0x1F
            if ra in pending:
                lo = w & 0xFFFF
                if lo >= 0x8000:
                    lo -= 0x10000
                globs.add(((pending[ra] << 16) + lo) & 0xFFFFFFFF)
                continue
        if op == 7:                                   # mulli
            imm = w & 0xFFFF
            if imm >= 0x8000:
                imm -= 0x10000
            if abs(imm) > 1:
                strides.add(imm)
            continue
        if op in DFORM:
            ra = (w >> 16) & 0x1F
            if ra != 3:                               # only the first argument
                continue
            off = w & 0xFFFF
            if off >= 0x8000:
                off -= 0x10000
            if off < 0:
                continue
            offsets.add(off)
            if op in STORES:
                stores.add(off)
                if off == 0 and any(True for _ in ()):
                    pass
                if off == 0:
                    # a store to +0 of something built from lis/addi is a
                    # vtable; a store of a computed value is not
                    rs = (w >> 21) & 0x1F
                    for j in range(max(0, i - 8), i):
                        w2 = words[j]
                        if (w2 >> 26) == 14 and ((w2 >> 21) & 0x1F) == rs:
                            vtable = True
    span = max(offsets) if offsets else 0
    return offsets, stores, span, vtable, strides, globs


def main(argv):
    img = Image()
    inv = dict(load_inventory())

    done, matched = {}, set()
    for fn, is_manifest in (("src/manifest.txt", True),
                            ("src/attempts.txt", False)):
        p = ROOT / fn
        if not p.exists():
            continue
        for line in p.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line or len(line.split()) < 2:
                continue
            f = line.split()
            try:
                a = int(f[1], 16)
            except ValueError:
                continue
            done[a] = Path(f[0]).name
            if is_manifest:
                matched.add(a)

    # What globals and vtables do the MATCHED functions already touch?
    seen_globals = defaultdict(set)
    for a in matched:
        r = analyse(img, a, inv.get(a, 0))
        if r:
            for g in r[5]:
                seen_globals[g].add(a)

    args = [a for a in argv[1:] if not a.startswith("--")]
    if args:
        va = int(args[0], 16)
        r = analyse(img, va, inv.get(va, 0))
        if not r:
            print("could not read %08X" % va)
            return 1
        offsets, stores, span, vtable, strides, globs = r
        print("sub_%08X  %d bytes" % (va, inv.get(va, 0)))
        print("  distinct field offsets on the first argument: %d" % len(offsets))
        print("    %s" % " ".join("0x%X" % o for o in sorted(offsets)))
        print("  of those WRITTEN: %d" % len(stores))
        print("    %s" % " ".join("0x%X" % o for o in sorted(stores)))
        print("  span (a lower bound on sizeof): 0x%X" % span)
        print("  stores a vtable at +0: %s" % ("yes" if vtable else "no"))
        if strides:
            print("  mulli strides (EXACT element sizes): %s"
                  % " ".join(str(s) for s in sorted(strides)))
        if globs:
            print("  globals referenced:")
            for g in sorted(globs):
                who = seen_globals.get(g, set())
                tag = ("  <- also touched by %s"
                       % ", ".join(sorted(done[x] for x in who))) if who else ""
                print("     %08X%s" % (g, tag))
        return 0

    cands = []
    p = ROOT / "build/candidates.txt"
    for line in p.read_text().splitlines():
        if not line.startswith("#") and line.split():
            cands.append(int(line.split()[0], 16))
    # non-leaf functions are not in candidates.txt, and constructors often
    # call something; walk the whole inventory instead and filter by scope.
    attr = {}
    q = ROOT / "build/attribution.txt"
    if q.exists():
        for line in q.read_text().splitlines():
            if line.startswith("#"):
                continue
            f = line.split()
            if len(f) >= 3:
                attr[int(f[0], 16)] = f[2]

    calls = defaultdict(int)
    for line in (ROOT / "build/callgraph.txt").read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) == 2:
            try:
                calls[int(f[1], 16)] += 1
            except ValueError:
                pass

    rows = []
    for va, size in inv.items():
        if va in done or attr.get(va) != "UNKNOWN" or not (16 <= size <= 400):
            continue
        r = analyse(img, va, size)
        if not r:
            continue
        offsets, stores, span, vtable, strides, globs = r
        if len(stores) < 4 and not strides:
            continue
        shared = sum(1 for g in globs if g in seen_globals)
        score = (len(stores) * 3 + len(offsets) + (12 if vtable else 0)
                 + (10 if strides else 0) + shared * 15
                 + min(calls.get(va, 0), 40))
        rows.append((score, va, size, len(offsets), len(stores), span,
                     vtable, sorted(strides), shared, calls.get(va, 0)))
    rows.sort(reverse=True)

    print("STRUCTURAL TARGETS -- what matching this would pin down\n")
    print("  address    bytes  fields  written  span   vt  stride     "
          "shared  callers")
    for (score, va, size, noff, nst, span, vt, strides, shared,
         nc) in rows[:25]:
        print("  %08X  %5d  %6d  %7d  0x%-4X %-3s %-10s %6d  %d"
              % (va, size, noff, nst, span, "yes" if vt else "-",
                 ",".join(str(s) for s in strides) or "-", shared, nc))
    print("")
    print("`shared` counts globals this function references that an ALREADY")
    print("MATCHED function also references -- a type identity supported by")
    print("evidence rather than assumed, which is what README gap 2 wants.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
