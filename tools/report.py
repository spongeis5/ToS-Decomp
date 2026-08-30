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

  `complete_code` MEANS LINKED, NOT MATCHED. decomp.dev shows a "decompiled"
  percentage and a separate "fully linked" one, and `complete` in objdiff is
  the second of those. It was hardcoded to zero for as long as that was the
  truth -- `tools/build.py` splices, and reporting 0.4% linked would have been
  false.

  It is no longer hardcoded. `tools/link.py` hands contiguous runs of matched
  functions to the retail `link.exe` 9.00.8153, places them at their retail
  addresses and compares byte for byte, and it owns the question of which
  units that leaves complete:

      a unit is complete when its object defines no function the manifest
      does not name, AND every one of those functions is in a run link.py
      linked, placed and found identical.

  Both clauses do work. The first excludes 61 of 406 units whose file defines
  a helper written to shape the caller's codegen and never compared to
  anything -- the splice never notices, because it only writes the named
  function, but a link takes the whole object. The second is the difference
  between matched and linked.

  This file does not decide it. It imports `link.complete_sources()`, because
  four earlier tools reimplemented a comparison another tool owned and all
  four disagreed with the owner in the direction that gets believed.

  When the link has not been run, `complete_code` is reported as 0 AND SAID
  TO BE UNMEASURED. Nothing linked and never asked are different states.

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
# ONE definition of the split, in tools/category.py. Four tools carried their
# own copy of the generated-prefix tuple, which is how a split silently stops
# agreeing; adding a third category to four places separately would have been
# the fifth time this project paid for that.
from category import category as _category, CATEGORIES


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
             total_units, complete_units, fuzzy, complete_code=0):
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
        # `complete` means LINKED, not matched -- see the module docstring.
        # It was hardcoded 0 for as long as nothing was linked; it is now
        # supplied by tools/link.py, which is the tool that owns the question.
        "complete_code": complete_code,
        "complete_code_percent": round(
            100.0 * complete_code / total_code if total_code else 0.0, 4),
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

    # WHICH UNITS ARE COMPLETE is link.py's question, not this file's. It is
    # the tool that runs link.exe, so it is the only thing that knows whether
    # a unit's functions were actually laid out at their retail addresses.
    # Deciding it here would be the fifth tool in this project to reimplement
    # a comparison another tool owns, and the previous four all disagreed with
    # the owner in the direction that gets believed.
    import link as linker
    complete_src, why_not, link_err = linker.complete_sources()
    if link_err:
        complete_src = set()

    units = []
    cat_tot = defaultdict(lambda: [0, 0, 0, 0])   # code, matched, fn, mfn
    matched_code = matched_fn = complete_code = 0
    complete_units = 0

    for src in sorted(by_file):
        cat = _category(src)
        gen = (cat != "handwritten")      # objdiff's "not worth a human's eye"
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
        is_complete = src in complete_src
        if is_complete:
            complete_units += 1
            complete_code += u_code
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
                                 1 if is_complete else 0,
                                 100.0 * u_matched / u_code if u_code else 0,
                                 u_code if is_complete else 0),
            "sections": [],
            "functions": fns,
            "metadata": {
                # `complete` means LINKED: every function this file defines is
                # matched AND was laid out by link.exe at its retail address,
                # byte-identical. tools/link.py decides it; see its
                # complete_sources().
                "complete": is_complete,
                "source_path": src.replace("\\", "/"),
                "progress_categories": [cat],
                "auto_generated": gen,
            },
        })

    fuzzy = 100.0 * matched_code / total_code if total_code else 0.0
    report = {
        "version": 1,
        "measures": measures(total_code, matched_code, total_fn, matched_fn,
                             len(units), complete_units, fuzzy,
                             complete_code),
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
    # The term that reconciles this denominator with objdiff-cli's. objdiff
    # can only count the units it is given and a unit is a function, so its
    # total is the sum of unit sizes: COMPILED lengths where a source exists,
    # inventory extents elsewhere. Ours is the whole section. The two differ
    # by the .text bytes belonging to no function, plus this -- the inventory
    # being wrong about the functions we do have. Printed so verify.py can
    # reconcile them exactly instead of allowing a vague tolerance.
    sourced_delta = sum(inv.get(a, 0) - csize.get(a, inv.get(a, 0))
                        for a in set(a for _s, a, _y, _f in matched + attempts))
    print("  inventory extent minus compiled length, over sourced "
          "functions: %d" % sourced_delta)
    if link_err:
        print("  complete_code     NOT MEASURED -- %s" % link_err)
        print("                    reported as 0, which is a floor and not a")
        print("                    finding. Run the link and read it again.")
    else:
        print("  complete_code     %d of %d  (%.4f%%) in %d complete unit(s)"
              % (complete_code, total_code,
                 100.0 * complete_code / total_code if total_code else 0.0,
                 complete_units))
        print("                    complete = every function the file defines")
        print("                    is matched AND was placed by link.exe at")
        print("                    its retail address, byte-identical")
    for cid, name in CATEGORIES:
        c = cat_tot[cid]
        print("  %-12s %d function(s), %d byte(s) matched"
              % (cid, c[3], c[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
