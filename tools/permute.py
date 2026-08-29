"""Try several source shapes for one target and report how each scores.

Matching is largely a search over ways of writing the same thing. Doing that
one edit at a time is slow; this compiles a set of candidate bodies against
one target and ranks them by how many instruction words agree.

    python tools/permute.py <variants.py> <address>
    python tools/permute.py <variants.py> <address> --flags /O2,/Os,...
    python tools/permute.py <variants.py> <address> --both

The variants file must define BODIES: a list of (name, source) pairs. Each
source is a complete translation unit.

Prints the best variant's full diff so the next edit has something to work
from, and writes every variant's score so a shape that got worse is on the
record rather than forgotten.

TWO THINGS THIS GOT WRONG for a long time, both of which made it report
confidently on the wrong axis:

  It compared RAW BYTES, discarding the relocation mask it had already
  computed. An object names its symbols by placeholder, so a relocated word
  differs from the image by construction; counting those as mismatches meant
  no function with a relocation could reach full marks, and the ranking
  quietly preferred shapes with fewer relocations over shapes with better
  code.

  It hardwired `/O2`. 38 of the matches in this project need `/O2 /Os`, and
  for those every variant here scored the same and the search looked
  exhausted when it had not started. `--both` compiles each variant at both
  levels and reports the better, which is the right default when the level
  is not yet known -- and when the two levels fail in DIFFERENT places, that
  is itself the finding (see `sub_825E35E0` in MATCHED.md).
"""

import importlib.util
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
from match import parse_flags
import ppcdis
import xdkcc

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/permute")
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def compile_src(text, flags=None):
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "v.cpp"
    src.write_text(text)
    blob, err = xdkcc.compile_obj(src, WORK / "v.obj", flags or FLAGS, WORK)
    if blob is None:
        return None, (err or "").splitlines()[:3]
    fns = coff_functions(blob)
    if not fns:
        return None, ["no PowerPC function in the object"]
    sym, code, mask = max(fns, key=lambda f: len(f[1]))
    code, mask = trim_padding(code, mask)
    return (sym, code, mask), None


def score(code, mask, tbytes, tsize):
    """-> (identical, compared, relocated). RELOCATED WORDS ARE NOT COMPARED.

    This used to compare raw bytes and discard the mask that `compile_src`
    had already computed. An object refers to its symbols by placeholder, so
    every relocated word differs from the image BY CONSTRUCTION -- counting
    those as mismatches meant no function with a relocation could ever score
    full marks here, and, worse, that the ranking preferred shapes with
    FEWER relocations rather than shapes with better code. A variant that
    got everything right still looked wrong.

    Three tools in this project have now disagreed with verify.py by
    reimplementing this comparison, always in the direction that gets
    believed. The mask is the whole difference, so it is threaded through
    and the relocated count is reported separately rather than folded in.
    """
    n = min(len(code), tsize) // 4
    same = compared = reloc = 0
    for i in range(n):
        if not all(mask[i * 4:i * 4 + 4]):
            reloc += 1
            continue
        compared += 1
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", code, i * 4)[0]
        if a == b:
            same += 1
    return same, compared, reloc


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    spec = importlib.util.spec_from_file_location("variants", argv[1])
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    target = int(argv[2], 16)

    img = Image()
    sizes = dict(load_inventory())
    if target not in sizes:
        print("%08X is not a known function start" % target)
        return 1
    tsize = sizes[target]
    tbytes = img.read(target, tsize)

    print("target %08X, %d byte(s), %d word(s)\n" % (target, tsize, tsize // 4))
    # Which optimisation levels to try. Same comma-or-whitespace parsing as
    # match.py, because two tools disagreeing about one string's format is
    # how `--flags /O2,/Os,...` came to fail as a LINKER error.
    levels = [("/O2", list(FLAGS))]
    if "--flags" in argv:
        levels = [("custom", parse_flags(argv[argv.index("--flags") + 1]))]
    if "--both" in argv:
        levels = [("/O2", list(FLAGS)),
                  ("/O2 /Os", parse_flags("/O2,/Os,/Gy,/GS-,/fp:fast"))]

    results = []
    for name, text in mod.BODIES:
        for lname, lflags in levels:
            got, err = compile_src(text, lflags)
            label = name if len(levels) == 1 else "%s [%s]" % (name, lname)
            if got is None:
                print("  %-40s DID NOT COMPILE: %s"
                      % (label, err[0] if err else "?"))
                continue
            sym, code, mask = got
            same, compared, reloc = score(code, mask, tbytes, tsize)
            sz = "%d B" % len(code)
            flag = ("" if len(code) == tsize
                    else "  <- SIZE %+d" % (len(code) - tsize))
            rl = "  (%d relocated, not compared)" % reloc if reloc else ""
            print("  %-40s %5s  %2d/%d words%s%s"
                  % (label, sz, same, compared, flag, rl))
            # Recorded INSIDE the level loop. Appending outside it takes the
            # last level's values -- or, after a `continue` on a compile
            # failure, the previous VARIANT's, which is a score attributed to
            # source that never produced it.
            results.append((same, compared, len(code) == tsize, label, code))

    if not results:
        print("no variant produced a function to compare.")
        return 2
    results.sort(key=lambda r: (r[2], r[0]), reverse=True)
    best_same, best_cmp, best_size_ok, best_name, best_code = results[0]
    print("\nbest: %s  (%d of %d compared word(s))"
          % (best_name, best_same, best_cmp))
    # The denominator is COMPARED words, not total: with relocated words in
    # the function, `same` can never reach the total and an exact match would
    # be reported as a near-miss forever.
    if best_cmp and best_same == best_cmp and best_size_ok:
        print("EXACT MATCH of every non-relocated word.")
        return 0

    print("\ndiff for the best variant:")
    n = min(len(best_code), tsize) // 4
    for i in range(n):
        va = target + i * 4
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", best_code, i * 4)[0]
        if a == b:
            continue
        ta = ppcdis.words([a], va)[0][2]
        tb = ppcdis.words([b], va)[0][2]
        print("  %08X  want %08x  %-30s" % (va, a, ta))
        print("            got  %08x  %-30s" % (b, tb))
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
