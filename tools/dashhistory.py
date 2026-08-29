"""Reconstruct the matched-function history from git, for the dashboard.

    python tools/dashhistory.py

Every commit carries `src/manifest.txt`, so the whole curve is recoverable
exactly -- no journal to keep and nothing to retype. Bytes come from the
CURRENT inventory applied to each commit's addresses, which is not an
approximation: the inventory is a property of the retail image, not of our
progress, and a function's extent does not change because we matched it.

THE SPLIT IS THE POINT. Plotted as one line, the jump from 181 to 559 in a
single commit reads as a breakthrough. It was `gen_typeids.py` -- 346
one-expression stubs generated from their own encodings. Hand-written and
generated are tracked separately here for the same reason MATCHED.md keeps
them apart: added together they say something false about how much of this
game has been read.
"""

import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import load_inventory

ROOT = Path(__file__).resolve().parent.parent
GENERATED = ("vt_typeid_", "vt_const_", "vt_acc_")


def git(*args):
    r = subprocess.run(["git"] + list(args), cwd=str(ROOT),
                       capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def main():
    inv = dict(load_inventory())
    log = git("log", "--format=%h %at", "--reverse")
    if log is None:
        print("not a git repository, or git is unavailable.")
        return 1

    points = []
    for line in log.splitlines():
        if not line.strip():
            continue
        sha, _, when = line.partition(" ")
        text = git("show", "%s:src/manifest.txt" % sha)
        if text is None:
            continue                       # manifest did not exist yet
        hand = gen = 0
        hb = gb = 0
        for row in text.splitlines():
            row = row.split("#")[0].strip()
            if not row:
                continue
            f = row.split()
            if len(f) < 2:
                continue
            try:
                addr = int(f[1], 16)
            except ValueError:
                continue
            n = inv.get(addr, 0)
            if Path(f[0]).name.startswith(GENERATED):
                gen += 1
                gb += n
            else:
                hand += 1
                hb += n
        points.append({"sha": sha, "t": int(when),
                       "hand": hand, "gen": gen,
                       "hand_bytes": hb, "gen_bytes": gb})

    print("%d commit(s) carry a manifest" % len(points))
    if points:
        first, last = points[0], points[-1]
        print("  from %d hand / %d generated" % (first["hand"], first["gen"]))
        print("  to   %d hand / %d generated" % (last["hand"], last["gen"]))
        span = (last["t"] - first["t"]) / 3600.0
        print("  over %.1f hour(s)" % span)
        big = max(range(1, len(points)),
                  key=lambda i: (points[i]["hand"] + points[i]["gen"])
                  - (points[i - 1]["hand"] + points[i - 1]["gen"]))
        p, q = points[big - 1], points[big]
        print("  largest single step: %s, %d -> %d (%d of it generated)"
              % (q["sha"], p["hand"] + p["gen"], q["hand"] + q["gen"],
                 q["gen"] - p["gen"]))

    out = ROOT / "build/dash_history.json"
    out.write_text(json.dumps(points), encoding="utf-8")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
