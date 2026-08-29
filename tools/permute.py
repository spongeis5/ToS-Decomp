"""Try several source shapes for one target and report how each scores.

Matching is largely a search over ways of writing the same thing. Doing that
one edit at a time is slow; this compiles a set of candidate bodies against
one target and ranks them by how many instruction words agree.

    python tools/permute.py <variants.py> <address>

The variants file must define BODIES: a list of (name, source) pairs. Each
source is a complete translation unit.

Prints the best variant's full diff so the next edit has something to work
from, and writes every variant's score so a shape that got worse is on the
record rather than forgotten.
"""

import importlib.util
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
import ppcdis
import xdkcc

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/permute")
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def compile_src(text):
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "v.cpp"
    src.write_text(text)
    blob, err = xdkcc.compile_obj(src, WORK / "v.obj", FLAGS, WORK)
    if blob is None:
        return None, (err or "").splitlines()[:3]
    fns = coff_functions(blob)
    if not fns:
        return None, ["no PowerPC function in the object"]
    sym, code, mask = max(fns, key=lambda f: len(f[1]))
    code, mask = trim_padding(code, mask)
    return (sym, code), None


def score(code, tbytes, tsize):
    n = min(len(code), tsize) // 4
    same = 0
    for i in range(n):
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", code, i * 4)[0]
        if a == b:
            same += 1
    return same, tsize // 4


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
    results = []
    for name, text in mod.BODIES:
        got, err = compile_src(text)
        if got is None:
            print("  %-34s DID NOT COMPILE: %s" % (name, err[0] if err else "?"))
            continue
        sym, code = got
        same, total = score(code, tbytes, tsize)
        sz = "%d B" % len(code)
        flag = "" if len(code) == tsize else "  <- SIZE %+d" % (len(code) - tsize)
        print("  %-34s %5s  %2d/%d words%s" % (name, sz, same, total, flag))
        results.append((same, len(code) == tsize, name, code))

    if not results:
        return 2
    results.sort(key=lambda r: (r[1], r[0]), reverse=True)
    best_same, best_size_ok, best_name, best_code = results[0]
    print("\nbest: %s  (%d/%d)" % (best_name, best_same, tsize // 4))
    if best_same == tsize // 4 and best_size_ok:
        print("EXACT MATCH")
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
