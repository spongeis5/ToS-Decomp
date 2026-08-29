"""Emit a decomp.dev-compatible progress report.

    python tools/report.py            write build/report.json
    python tools/report.py --print    also print the headline measures

decomp.dev renders objdiff's progress-report schema into a project page, a
badge and PR comments. The schema is `objdiff-core/protos/report.proto`;
this writes the JSON form of it directly, because objdiff-cli is a Rust
binary that is not held here and every number it would compute is already
known to this repository.

TWO PLACES WHERE THE HONEST ANSWER IS NOT THE FLATTERING ONE, and both are
worth stating because a report is read by people who cannot check it.

  `complete_code` IS ZERO. decomp.dev shows a "decompiled" percentage and a
  separate "fully linked" one, and `complete` in objdiff means the object is
  LINKED, not that its bytes match. tools/build.py says what this project
  actually does, every run: "This is a SPLICE, not yet a LINK. The
  undecompiled code is copied rather than assembled from objects." Reporting
  0.4% linked would be false. Matched code is reported as matched; linked is
  reported as zero until there is a link.

  A UNIT IS A SOURCE FILE, not a function. objdiff.json lists one unit per
  function because that is the useful granularity for a visual diff, but a
  report unit is a translation unit, and counting 1,242 "units" against a
  game with a few thousand real ones would misstate the denominator. Units
  here are the .cpp files, with their functions listed inside.

THE DENOMINATOR. `total_code` is the whole of `.text`, 8,467,964 bytes,
including the third of it that is Microsoft's own libraries and the
middleware for which no archive is held. That is the honest denominator for
"how much of this image is reproduced from source in this repository", which
is the question the number answers.

THE NUMERATOR IS THE COMPILED LENGTH, not the inventory extent. Written the
easy way this reported 34,340 bytes where `build.py` reports 34,096, because
the inventory is wrong in both directions -- short where a tail call's dead
`blr` was not counted, long where one `.pdata` row covers several frameless
bodies. Two numbers for the same fact is the drift that produced most of
this project's tooling bugs, so the size of a matched function is what the
compiler actually emitted, exactly as `matched_table.py` and `build.py`
measure it.
"""

import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/report.json"
GENERATED = ("vt_typeid_", "vt_const_", "vt_acc_")

CATEGORIES = [
    ("handwritten", "Hand-written from disassembly"),
    ("generated", "Generated from encodings"),
]


def rows(path):
    out = []
    for line in (ROOT / path).read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) < 2:
            continue
        try:
            addr = int(f[1], 16)
        except ValueError:
            continue
        sym, flags = None, None
        for extra in f[2:]:
            if extra.startswith("flags="):
                flags = extra[len("flags="):]
            elif extra != "-":
                sym = extra
        out.append((f[0], addr, sym, flags))
    return out


def compiled_sizes(all_rows):
    """{address: bytes the compiler emitted}, via matched_table's own path.

    Not a second implementation: `matched_table.compiled_size` is what
    MATCHED.md's byte column is built from, and reusing it is the only way
    this report and that table cannot disagree. The compile memo makes it
    cheap -- every object is already on disk from the last build.
    """
    import matched_table
    out = {}
    for src, addr, sym, flags in all_rows:
        n = matched_table.compiled_size(src, sym, flags, addr)
        if n is not None:
            out[addr] = n
    return out


def measures(total_code, matched_code, total_fn, matched_fn,
             total_units, complete_units, fuzzy):
    return {
        "fuzzy_match_percent": round(fuzzy, 4),
        "total_code": total_code,
        "matched_code": matched_code,
        "matched_code_percent": round(
            100.0 * matched_code / total_code if total_code else 0.0, 4),
        "total_data": 0,
        "matched_data": 0,
        "matched_data_percent": 0.0,
        "total_functions": total_fn,
        "matched_functions": matched_fn,
        "matched_functions_percent": round(
            100.0 * matched_fn / total_fn if total_fn else 0.0, 4),
        # Zero, and deliberately. See the module docstring: nothing links yet.
        "complete_code": 0,
        "complete_code_percent": 0.0,
        "complete_data": 0,
        "complete_data_percent": 0.0,
        "total_units": total_units,
        "complete_units": complete_units,
    }


def main(argv):
    img = Image()
    inv = dict(load_inventory())
    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = tlo + (text["vsize"] or text["rawsz"])
    total_code = text["vsize"] or text["rawsz"]
    total_fn = sum(1 for a in inv if tlo <= a < thi)

    matched = rows("src/manifest.txt")
    attempts = rows("src/attempts.txt")
    csize = compiled_sizes(matched + attempts)

    by_file = defaultdict(list)
    for src, addr, _s, _f in matched:
        by_file[src].append((addr, True))
    for src, addr, _s, _f in attempts:
        by_file[src].append((addr, False))

    units = []
    cat_tot = defaultdict(lambda: [0, 0, 0, 0])   # code, matched, fn, mfn
    matched_code = matched_fn = 0
    complete_units = 0

    for src in sorted(by_file):
        gen = Path(src).name.startswith(GENERATED)
        cat = "generated" if gen else "handwritten"
        fns = []
        u_code = u_matched = 0
        u_ok = 0
        for addr, ok in sorted(by_file[src]):
            # The COMPILED length; the inventory extent only where the
            # function has no source to compile.
            size = csize.get(addr, inv.get(addr, 0))
            u_code += size
            if ok:
                u_matched += size
                u_ok += 1
            fns.append({
                "name": "sub_%08X" % addr,
                "size": size,
                "fuzzy_match_percent": 100.0 if ok else 0.0,
                "address": addr - tlo,
                "metadata": {"virtual_address": addr},
            })
        all_ok = (u_ok == len(fns))
        if all_ok:
            complete_units += 1
        matched_code += u_matched
        matched_fn += u_ok
        c = cat_tot[cat]
        c[0] += u_code
        c[1] += u_matched
        c[2] += len(fns)
        c[3] += u_ok

        units.append({
            "name": Path(src).stem,
            "measures": measures(u_code, u_matched, len(fns), u_ok, 1,
                                 1 if all_ok else 0,
                                 100.0 * u_matched / u_code if u_code else 0),
            "sections": [],
            "functions": fns,
            "metadata": {
                # `complete` here would mean LINKED. Nothing is.
                "complete": False,
                "source_path": src.replace("\\", "/"),
                "progress_categories": [cat],
                "auto_generated": gen,
            },
        })

    fuzzy = 100.0 * matched_code / total_code if total_code else 0.0
    report = {
        "version": 1,
        "measures": measures(total_code, matched_code, total_fn, matched_fn,
                             len(units), complete_units, fuzzy),
        "units": units,
        "categories": [
            {"id": cid, "name": name,
             "measures": measures(cat_tot[cid][0], cat_tot[cid][1],
                                  cat_tot[cid][2], cat_tot[cid][3],
                                  sum(1 for u in units
                                      if cid in u["metadata"]
                                      ["progress_categories"]),
                                  0,
                                  100.0 * cat_tot[cid][1] / cat_tot[cid][0]
                                  if cat_tot[cid][0] else 0.0)}
            for cid, name in CATEGORIES
        ],
    }

    OUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("wrote %s" % OUT)
    print("  %d unit(s) (source files), %d function(s)"
          % (len(units), sum(len(u["functions"]) for u in units)))
    print("  matched_code      %d of %d  (%.4f%%)"
          % (matched_code, total_code, fuzzy))
    print("  matched_functions %d of %d" % (matched_fn, total_fn))
    print("  complete_code     0  -- nothing is LINKED yet; build.py splices")
    for cid, name in CATEGORIES:
        c = cat_tot[cid]
        print("  %-12s %d function(s), %d byte(s) matched"
              % (cid, c[3], c[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
