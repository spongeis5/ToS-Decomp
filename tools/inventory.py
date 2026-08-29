"""Build the complete function inventory: .pdata UNION Ghidra.

`.pdata` is the compiler's unwind table and misses leaf functions that need
no unwind record.  Ghidra's analysis finds those by following flow.  Every
percentage this project has quoted used the .pdata count as its denominator,
which is ~18% short -- 21,238 against the 25,737 Ghidra knows about.

Sizes come from .pdata where a row exists (it is the compiler's own answer)
and from Ghidra's computed body otherwise, and which source supplied each is
recorded so the two can never be silently conflated.
"""

import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import load_functions

GHIDRA = Path("build/ghidra_fn_v2.txt")
OUT = Path("build/functions_all.txt")


def main():
    pd = dict(load_functions())
    if not GHIDRA.exists():
        print("%s missing -- run DumpFunctions first" % GHIDRA, file=sys.stderr)
        return 1
    gh = {}
    for line in GHIDRA.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        gh[int(f[0], 16)] = int(f[1])

    all_addrs = sorted(set(pd) | set(gh))
    st = Counter()
    rows = []
    for a in all_addrs:
        if a in pd:
            rows.append((a, pd[a], "pdata"))
            st["pdata"] += 1
        else:
            rows.append((a, gh[a], "ghidra"))
            st["ghidra_only"] += 1

    print(".pdata functions        %6d" % len(pd))
    print("ghidra functions        %6d" % len(gh))
    print("union                   %6d" % len(all_addrs))
    print("  size from .pdata      %6d" % st["pdata"])
    print("  size from Ghidra only %6d" % st["ghidra_only"])
    missing_from_gh = len(set(pd) - set(gh))
    print("  in .pdata but not Ghidra %3d" % missing_from_gh)

    with OUT.open("w") as f:
        f.write("# address size source\n")
        for a, s, src in rows:
            f.write("%08X %8d %s\n" % (a, s, src))
    print("\nwrote %s (%d rows)" % (OUT, len(rows)))

    # How much of .text does the union cover?
    total = sum(s for _a, s, _x in rows)
    print("bytes covered by the union: %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
