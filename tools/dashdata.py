"""Collect the numbers the dashboard shows, straight from the tools."""
import collections
import json
import subprocess
import sys
from pathlib import Path

# Derived from this file's own location, like every other tool here. It was
# an absolute path to one machine, which both broke for anyone who cloned
# the repository and published the author's local account name.
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from peimage import Image, load_inventory

img = Image()
inv = dict(load_inventory())
text = next(s for s in img.sections if s["name"] == ".text")
TLO = text["va"]
TSIZE = text["vsize"] or text["rawsz"]

GEN = ("vt_typeid_", "vt_const_", "vt_acc_")


def rows(p):
    out = []
    for line in (ROOT / p).read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) < 2:
            continue
        try:
            out.append((f[0], int(f[1], 16)))
        except ValueError:
            pass
    return out


matched = rows("src/manifest.txt")
attempts = rows("src/attempts.txt")

gen = [r for r in matched if Path(r[0]).name.startswith(GEN)]
hand = [r for r in matched if not Path(r[0]).name.startswith(GEN)]

# Bytes: use the inventory extent, which is what build.py splices.
def nbytes(rs):
    return sum(inv.get(a, 0) for _s, a in rs)


# BYTE MAP: .text bucketed into cells; each cell is the fraction matched.
CELLS = 1800
per = TSIZE / float(CELLS)
cover = [0.0] * CELLS
for _s, a in matched:
    n = inv.get(a, 0)
    if not n:
        continue
    lo = a - TLO
    for b in range(lo, lo + n, 64):
        c = int(b / per)
        if 0 <= c < CELLS:
            cover[c] += min(64, lo + n - b)
cells = [min(1.0, c / per) for c in cover]

# ATTRIBUTION in bytes.
attr = collections.Counter()
libs = collections.Counter()
for line in (ROOT / "build/attribution.txt").read_text().splitlines():
    if line.startswith("#"):
        continue
    f = line.split()
    if len(f) < 3:
        continue
    attr[f[2]] += int(f[1])
    if f[2] == "lib" and len(f) >= 4:
        libs[f[3]] += int(f[1])

out = {
    "text_bytes": TSIZE,
    "built_bytes": nbytes(matched),
    "matched_total": len(matched),
    "matched_hand": len(hand),
    "matched_gen": len(gen),
    "hand_bytes": nbytes(hand),
    "gen_bytes": nbytes(gen),
    "attempts": len(attempts),
    "inventory_rows": len(inv),
    "cells": [round(c, 3) for c in cells],
    "attribution": dict(attr),
    "libs": dict(libs.most_common(10)),
}
# build/, where every other generated artifact lives and which is
# gitignored. This wrote to a session scratchpad under the author's home
# directory: not reproducible anywhere else, and it published an account
# name and a session id.
p = ROOT / "build/dash.json"
p.write_text(json.dumps(out), encoding="utf-8")
print("wrote %s" % p)
for k, v in out.items():
    if k != "cells":
        print("%-16s %s" % (k, v))
print("cells           %d (nonzero %d)"
      % (len(cells), sum(1 for c in cells if c > 0)))
