"""Climb the call graph: which CALLER should be matched next?

    python tools/climb.py                 the best next steps, ranked
    python tools/climb.py 82662E08        the callers of one function
    python tools/climb.py --tree 82662E08 what calls it, two levels up

Matching leaves scattered across the image proves the toolchain works. It
does not build anything. Going UP from a matched function -- leaf to branch
to bough -- produces a connected subtree, and a connected subtree is the
first thing that could ever be linked as a unit.

The ranking answers one question: **how much of this caller do we already
know?** A function whose callees are all matched, or all attributed to a
library and therefore out of scope, can be written with every name already
decided. One whose callees are unknown needs them invented, and every
invented name is a guess that a later match may contradict -- which is
exactly the drift `build.py`'s name check now catches.

    ready   = callees that are matched, or library, or an import
    unknown = callees that are none of those

Ranked by ready-fraction first and caller count second, because a caller with
many callers of its own is a bough rather than a twig.
"""

import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import load_inventory

ROOT = Path(__file__).resolve().parent.parent
XDK = [(0x82100000, 0x821294A0), (0x822F03E8, 0x82523A1C),
       (0x828A74A0, 0x82908510)]


def in_xdk(a):
    return any(lo <= a < hi for lo, hi in XDK)


def load():
    inv = dict(load_inventory())

    callees = defaultdict(set)
    callers = defaultdict(set)
    p = ROOT / "build/callgraph.txt"
    if not p.exists():
        print("build/callgraph.txt is missing -- run tools/discover.py.")
        sys.exit(1)
    for line in p.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) != 2:
            continue
        try:
            a, b = int(f[0], 16), int(f[1], 16)
        except ValueError:
            continue
        callees[a].add(b)
        callers[b].add(a)

    attr = {}
    p = ROOT / "build/attribution.txt"
    if p.exists():
        for line in p.read_text().splitlines():
            if line.startswith("#"):
                continue
            f = line.split()
            if len(f) >= 3:
                attr[int(f[0], 16)] = f[2]

    done = {}
    for fn in ("src/manifest.txt", "src/attempts.txt"):
        q = ROOT / fn
        if not q.exists():
            continue
        for line in q.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 2:
                try:
                    done[int(f[1], 16)] = (Path(f[0]).name,
                                           fn.endswith("manifest.txt"))
                except ValueError:
                    pass
    return inv, callees, callers, attr, done


def classify(a, attr, done):
    """What do we already know about this callee?"""
    if a in done:
        return "matched" if done[a][1] else "attempted"
    sig = attr.get(a)
    if sig == "lib":
        return "library"
    if sig in ("rtti_havok", "srcpath", "havok"):
        return "middleware"
    if in_xdk(a):
        return "xdk"
    return "unknown"


READY = ("matched", "library", "middleware", "xdk")


def main(argv):
    inv, callees, callers, attr, done = load()
    matched = {a for a, (_s, ok) in done.items() if ok}

    args = [a for a in argv[1:] if not a.startswith("--")]
    if args:
        seed = int(args[0], 16)
        depth = 2 if "--tree" in argv else 1
        show_callers(seed, 0, depth, inv, callees, callers, attr, done, set())
        return 0

    # Every caller of anything we have matched.
    cands = set()
    for m in matched:
        for c in callers.get(m, ()):
            if c in matched or c in done or in_xdk(c):
                continue
            if attr.get(c) in ("lib", "rtti_havok", "srcpath", "havok"):
                continue
            if c in inv:
                cands.add(c)

    rows = []
    for c in cands:
        kids = callees.get(c, set())
        counts = defaultdict(int)
        for k in kids:
            counts[classify(k, attr, done)] += 1
        ready = sum(counts[k] for k in READY)
        total = len(kids)
        frac = (ready / float(total)) if total else 1.0
        rows.append((frac, len(callers.get(c, ())), c, inv.get(c, 0),
                     ready, total, counts["matched"]))
    rows.sort(key=lambda r: (-r[0], -r[1], r[3]))

    print("CALLERS OF MATCHED FUNCTIONS -- the next step up")
    print("%d candidate(s); ranked by how much of each is already known.\n"
          % len(rows))
    print("  address    bytes  callers   callees known   of which matched")
    for frac, ncall, c, size, ready, total, nmatched in rows[:30]:
        print("  %08X  %5d  %6d   %3d of %-3d %3.0f%%      %d"
              % (c, size, ncall, ready, total, frac * 100, nmatched))
    print("")
    print("`climb.py <address>` lists one function's callers and callees with")
    print("what is known about each; `--tree` goes two levels up.")
    return 0


def show_callers(a, level, depth, inv, callees, callers, attr, done, seen):
    if a in seen or level > depth:
        return
    seen.add(a)
    pad = "  " * level
    tag = done.get(a)
    what = ("MATCHED as %s" % tag[0]) if (tag and tag[1]) else \
           ("attempted in %s" % tag[0]) if tag else classify(a, attr, done)
    print("%s%08X  %4d B  %3d caller(s)   %s"
          % (pad, a, inv.get(a, 0), len(callers.get(a, ())), what))
    if level == 0:
        kids = sorted(callees.get(a, ()))
        if kids:
            print("%s  calls:" % pad)
            for k in kids:
                print("%s    %08X  %4d B  %s"
                      % (pad, k, inv.get(k, 0), classify(k, attr, done)))
        print("%s  called by:" % pad)
    for c in sorted(callers.get(a, ())):
        show_callers(c, level + 1, depth, inv, callees, callers, attr, done,
                     seen)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
