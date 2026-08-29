"""Build the complete function inventory: .pdata UNION discovery.

`.pdata` is the compiler's unwind table and misses leaf functions that need no
unwind record. The other functions are found by `tools/discover.py`, a linear
branch sweep plus a data-pointer scan over the image.

**This used to union .pdata with Ghidra.** Measured head to head, discovery
wins on every axis that was checked, so Ghidra is now optional and kept only
as a cross-check:

    functions beyond .pdata     Ghidra 4,499      discovery 9,746
    of Ghidra's, rediscovered   --                99.7% (13 missed)
    call-graph edges            73,686            85,315
    sizes correct on the 15 functions whose true size is
    established by the reconstructing build
                                13 / 15           15 / 15
    wall clock                  a 12 GB headless  1.1 s
                                run
    VMX128                      cannot decode it  decodes it

The two sizes Ghidra gets wrong are the two tail-call functions: it computes a
body from REACHABLE code, so the unreachable `blr` MSVC appends after a tail
call is not counted, and the size comes out 4 short. 171 functions in the
image have that shape.

Sizes come from `.pdata` where a row exists -- it is the compiler's own answer
-- and otherwise from the extent to the next start with padding trimmed, which
agrees with `.pdata` on 97.0% of the rows that can be compared. Which source
supplied each is recorded so the two are never silently conflated.

    python tools/inventory.py             .pdata + discovery  (default)
    python tools/inventory.py --ghidra    the old union, for comparison
    python tools/inventory.py --addrtaken  ALSO fold in tools/addrtaken.py

`--addrtaken` is opt-in because it changes SIZES as well as adding rows, and
every derived file downstream was built against the current ones. It folds in
the 1,252 starts found by address-taken-in-code (FINDINGS 7r), and TRUNCATES
any existing row whose extent runs past one of them -- which is the point:
521 rows are too long because one `.pdata` unwind record can cover a run of
adjacent frameless bodies (7q).

After running it, `tools/attribute.py` and `tools/candidates.py` must be
re-run, in that order, or candidates.py silently drops everything missing
from a stale attribution.
"""

import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import load_functions

GHIDRA = Path("build/ghidra_fn_v2.txt")
ADDRTAKEN = Path("build/addrtaken.txt")
DISCOVERED = Path("build/discovered_inventory.txt")
OUT = Path("build/functions_all.txt")


def read_pairs(path):
    out = {}
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        out[int(f[0], 16)] = int(f[1])
    return out


def read_starts(path):
    out = []
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        out.append(int(line.split()[0], 16))
    return out


def trim(img, va, size):
    """Drop trailing alignment padding, as discover.py does."""
    import struct
    while size >= 8:
        raw = img.read(va + size - 4, 4)
        if raw is None:
            break
        w = struct.unpack(">I", raw)[0]
        if w not in (0, 0x60000000):
            break
        size -= 4
    return size


def main(argv):
    use_ghidra = "--ghidra" in argv[1:]
    use_taken = "--addrtaken" in argv[1:]
    src_path = GHIDRA if use_ghidra else DISCOVERED
    label = "ghidra" if use_ghidra else "discover"

    pd = dict(load_functions())
    if not src_path.exists():
        print("%s missing -- run %s first"
              % (src_path, "DumpFunctions" if use_ghidra
                 else "tools/discover.py"), file=sys.stderr)
        return 1
    other = read_pairs(src_path)

    all_addrs = sorted(set(pd) | set(other))
    st = Counter()
    rows = []
    for a in all_addrs:
        if a in pd:
            rows.append((a, pd[a], "pdata"))
            st["pdata"] += 1
        else:
            rows.append((a, other[a], label))
            st["other"] += 1

    print(".pdata functions          %6d" % len(pd))
    print("%-14s functions   %6d" % (label, len(other)))
    print("union                     %6d" % len(all_addrs))
    print("  size from .pdata        %6d" % st["pdata"])
    print("  size from %-8s only  %6d" % (label, st["other"]))
    print("  in .pdata but not %-8s %5d" % (label, len(set(pd) - set(other))))

    if use_taken:
        if not ADDRTAKEN.exists():
            print("%s missing -- run tools/addrtaken.py first" % ADDRTAKEN,
                  file=sys.stderr)
            return 1
        from peimage import Image
        img = Image()
        extra = [a for a in read_starts(ADDRTAKEN)
                 if a not in {r[0] for r in rows}]
        merged = sorted(rows + [(a, 0, "addrtaken") for a in extra],
                        key=lambda r: r[0])
        out2, shortened = [], 0
        for i, (a, s, src) in enumerate(merged):
            limit = (merged[i + 1][0] - a) if i + 1 < len(merged) else s
            if src == "addrtaken":
                s = trim(img, a, limit)
            elif limit < s:
                s = trim(img, a, limit)
                shortened += 1
            out2.append((a, s, src))
        rows = out2
        print("")
        print("--addrtaken: %d start(s) added, %d existing row(s) SHORTENED"
              % (len(extra), shortened))
        print("  Re-run tools/attribute.py then tools/candidates.py; a stale")
        print("  attribution makes candidates.py drop everything not in it.")

    with OUT.open("w") as f:
        f.write("# address size source\n")
        for a, s, src in rows:
            f.write("%08X %8d %s\n" % (a, s, src))
    print("")
    print("wrote %s (%d rows)" % (OUT, len(rows)))
    print("bytes covered by the union: %d" % sum(s for _a, s, _x in rows))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
