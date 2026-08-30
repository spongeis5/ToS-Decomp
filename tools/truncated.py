"""Which inventory rows are TRUNCATED by a jump table sitting inside them?

    python tools/truncated.py             the census
    python tools/truncated.py --check     non-zero if candidates.txt offers one

An MSVC switch puts its jump table in `.text`, immediately after the `bctr`
that reads it. Function discovery stops at the `bctr` -- it is a terminator
-- so the row records the bytes up to the table and the rest of the function,
everything after the table, is left out.

The consequence is not a slightly wrong size. It is that `batch.py` offers a
352-byte eleven-arm switch as a 64-byte function, and an agent spends its
budget on something that could not have matched at any size, because the
bytes it was asked to reproduce end in the middle of a control transfer.
Two of the forty candidates in one batch were this, and it was found by an
agent noticing rather than by a tool refusing.

WHERE THE TRUNCATION ACTUALLY IS. Not in the inventory: it records 352 and
624 for the two functions above, and `.pdata` agrees with it. It is in
`build/candidates.txt`, which carries its OWN size column -- 64 and 136 --
computed by `candidates.py` walking to the first terminator. `batch.py`
prints that column, so the disassembly an agent is handed stops at the
`bctr` even though every other tool in the project knows the function is
five times longer.

That makes this the fifth time two tools here have disagreed about the same
fact, and the fourth where the disagreement was silent and in the direction
that gets believed. So the check is not "is this row odd" but the general
one: **does candidates.txt agree with the inventory?** Any disagreement is
reported; the switch table in the gap is then the explanation rather than
the detector.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory, load_functions

ROOT = Path(__file__).resolve().parent.parent
SWITCHES = ROOT / "build/switch_tables.txt"
CANDIDATES = ROOT / "build/candidates.txt"


def switch_tables():
    """[(lo, hi)] for every jump table switches.py identified.

    This was the only one of five readers that parsed the length correctly,
    and it still read a recorded 0 as an empty range rather than as "the
    case count could not be recovered". One reader now, tools/switchtab.py.
    """
    import switchtab
    from peimage import Image as _Image
    return switchtab.Tables(_Image()).ranges


def main(argv):
    img = Image()
    inv = dict(load_inventory())
    pdata = dict(load_functions())
    tables = switch_tables()

    print("%d inventory row(s); %d .pdata row(s); %d switch table(s)"
          % (len(inv), len(pdata), len(tables)))
    if not tables:
        print("")
        print("build/switch_tables.txt is missing or empty. Run")
        print("tools/switches.py. Refusing to report 'no truncated rows',")
        print("because that is what this would print either way.")
        return 1

    if not CANDIDATES.exists():
        print("build/candidates.txt is missing. Run tools/candidates.py.")
        return 1
    cand = {}
    for line in CANDIDATES.read_text().splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) < 2:
            continue
        try:
            cand[int(f[0], 16)] = int(f[1])
        except ValueError:
            continue
    print("%d candidate row(s)" % len(cand))

    disagree = []
    for va, csize in sorted(cand.items()):
        isize = inv.get(va)
        if isize is None or isize == csize:
            continue
        lo, hi = va + min(csize, isize), va + max(csize, isize)
        inside = [(a, b) for a, b in tables if lo <= a < hi]
        disagree.append((va, csize, isize, pdata.get(va), inside))

    print("")
    print("%d of %d candidate row(s) DISAGREE with the inventory about size"
          % (len(disagree), len(cand)))
    with_table = [d for d in disagree if d[4]]
    print("  %d of the %d have a jump table inside the gap -- those are"
          % (len(with_table), len(disagree)))
    print("  switch functions whose row stops at the `bctr`")
    pd_agrees = sum(1 for d in disagree if d[3] is not None and d[3] == d[2])
    print("  %d of the %d have a .pdata extent agreeing with the INVENTORY,"
          % (pd_agrees, len(disagree)))
    print("  which is the tie-break: two independent sources against one")
    print("")
    if disagree:
        print("  address   candidates  inventory  .pdata  table(s) in the gap")
        for va, cs, isz, pd, inside in disagree[:30]:
            print("  %08X %10d %10s %7s  %s"
                  % (va, cs, isz, pd if pd is not None else "-",
                     " ".join("%08X+%d" % (a, b - a) for a, b in inside)
                     or "-"))
        if len(disagree) > 30:
            print("  ... %d more" % (len(disagree) - 30))

    if "--check" in argv and disagree:
        print("")
        print("FAIL: candidates.txt and the inventory disagree about %d"
              % len(disagree))
        print("function size(s). batch.py prints the candidates column, so")
        print("each of these is offered for matching with its disassembly")
        print("cut short -- and the bytes it asks for end in the middle of")
        print("a control transfer, which cannot match at any size.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
