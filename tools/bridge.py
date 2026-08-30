"""Which unmatched function would JOIN or EXTEND a linked run?

    python tools/bridge.py             the ranked list
    python tools/bridge.py 30          the first 30, with disassembly
    python tools/bridge.py --joins     only the ones that would MERGE two runs

Every other candidate picker here ranks by a property of the function --
caller count (`batch.py`), what is already known below it (`climb.py`), how
much layout it would pin (`layout.py`). None of them knows about runs, and a
run is now the thing that gets LINKED: `tools/link.py` hands contiguous runs
of matched functions to the retail linker, and a unit only counts as complete
when every function it defines sits inside one.

So this ranks by a property of the NEIGHBOURHOOD instead:

    a BRIDGE   sits between two matched runs and would merge them
    an EXTENSION touches one run on one side and would lengthen it
    a SEED     touches a single matched function, making a run of two

A bridge is worth far more than its own bytes. Closing an 84-byte hole
between two 168- and 364-byte runs does not add 84 bytes of linked code, it
adds 616 -- and it can turn a real translation unit, split across three
invented source files, into one span the linker lays out end to end.

The gap that matters is the one containing UNMATCHED FUNCTIONS. Adjacent
matched functions separated by 4 bytes are already one run: that is the
8-byte COMDAT alignment, measured 464 times out of 464, not a hole.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import link as L
import matched_table
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent


def callers():
    counts = {}
    p = ROOT / "build/callgraph.txt"
    if not p.exists():
        return counts
    for line in p.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) == 2:
            try:
                k = int(f[1], 16)
            except ValueError:
                continue
            counts[k] = counts.get(k, 0) + 1
    return counts


def switch_tables():
    """Inventory rows that are jump TABLES, not functions. Never candidates."""
    out = []
    p = ROOT / "build/switch_tables.txt"
    if not p.exists():
        return out
    for line in p.read_text().splitlines():
        f = line.split()
        if len(f) >= 2 and not line.startswith("#"):
            try:
                out.append((int(f[0], 16), int(f[1], 16)))
            except ValueError:
                pass
    return out


def analyse():
    """-> (runs, [candidate dicts]) sorted best first."""
    sized = L.compiled(matched_table.rows())
    runs = L.runs_of(sized)
    matched = dict((a, n) for a, n, _s, _y, _f in sized)
    inv = dict(load_inventory())
    tables = switch_tables()
    ncall = callers()

    # Every run's extent, and every matched function's run (or None).
    run_at = {}
    extents = []
    for r in runs:
        lo, hi = r[0][0], r[-1][0] + r[-1][1]
        extents.append((lo, hi, len(r)))
        for a, n, _s, _y, _f in r:
            run_at[a] = (lo, hi)

    # Unmatched inventory rows, by address.
    cand = []
    for addr in sorted(inv):
        if addr in matched:
            continue
        size = inv[addr]
        if any(t0 <= addr < t1 for t0, t1 in tables):
            continue
        end = addr + size

        # What is immediately before and after, allowing the 4-byte
        # alignment slack the linker itself inserts.
        before = after = None
        for a, n in matched.items():
            if 0 <= addr - (a + n) <= 4:
                before = a
            if 0 <= a - end <= 4:
                after = a
        if before is None and after is None:
            continue

        kind, gain = "seed", size
        lo_r = run_at.get(before)
        hi_r = run_at.get(after)
        if lo_r and hi_r:
            kind = "bridge"
            gain = hi_r[1] - lo_r[0]
        elif lo_r or hi_r:
            r = lo_r or hi_r
            kind = "extension"
            gain = (r[1] - r[0]) + size
        elif before is not None and after is not None:
            kind = "bridge"
            gain = size + matched[before] + matched[after]
        else:
            other = before if before is not None else after
            gain = size + matched[other]

        cand.append({"addr": addr, "size": size, "kind": kind, "gain": gain,
                     "before": before, "after": after,
                     "callers": ncall.get(addr, 0)})

    order = {"bridge": 0, "extension": 1, "seed": 2}
    cand.sort(key=lambda c: (order[c["kind"]], -c["gain"], c["size"]))
    return runs, cand


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    only_joins = "--joins" in argv
    show = int(args[0]) if args else 0

    runs, cand = analyse()
    if only_joins:
        cand = [c for c in cand if c["kind"] == "bridge"]

    kinds = {}
    for c in cand:
        kinds[c["kind"]] = kinds.get(c["kind"], 0) + 1
    print("%d unmatched function(s) touch a matched one, of %d run(s) of 2+"
          % (len(cand), len(runs)))
    for k in ("bridge", "extension", "seed"):
        if kinds.get(k):
            print("   %-10s %4d   %s"
                  % (k, kinds[k],
                     {"bridge": "would MERGE two runs",
                      "extension": "would lengthen one run",
                      "seed": "would make a new run of two"}[k]))
    print("")
    print("%-10s %6s %-10s %8s %8s  %s"
          % ("address", "bytes", "kind", "run gain", "callers", "neighbours"))
    for c in cand[:show or 40]:
        print("%08X %6d %-10s %8d %8d  %s%s"
              % (c["addr"], c["size"], c["kind"], c["gain"], c["callers"],
                 "after %08X" % c["before"] if c["before"] else "",
                 "  before %08X" % c["after"] if c["after"] else ""))
    if len(cand) > (show or 40):
        print("... and %d more" % (len(cand) - (show or 40)))

    if show:
        img = Image()
        sys.path.insert(0, str(ROOT / "tools"))
        import disasm
        print("")
        for c in cand[:show]:
            print("=" * 72)
            print("%08X  %d bytes  %s  would take a run to %d bytes"
                  % (c["addr"], c["size"], c["kind"], c["gain"]))
            print("=" * 72)
            try:
                print(disasm.disassemble_range(img, c["addr"],
                                               c["addr"] + c["size"]))
            except Exception as e:
                print("  (disassembly unavailable: %s)" % e)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
