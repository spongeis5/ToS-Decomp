"""Sweep compiler flags for one source against one target function.

    python tools/flagsweep.py src/init12.cpp 826C1480
    python tools/flagsweep.py src/init12.cpp 826C1480 --full

Source shape and compiler flags are two different search spaces and confusing
them wastes time. tools/permute.py searches shapes at fixed flags; this
searches flags at a fixed shape.

The space is built from the compiler's OWN option list (`build/cl_help.txt`,
from `cl /?`), not from desktop-MSVC habits. That matters here: this is the
PowerPC compiler and it has options desktop MSVC does not, notably

    /Ou   enable prescheduling

which is an instruction-SCHEDULING control -- the exact thing that both
stalled matches come down to. Sweeps written from x86 memory will never
contain it.

Reports every combination tried with its score, so a flag that made things
worse is on the record, and states the size of the space it searched. A sweep
that reports only its winner is indistinguishable from a sweep that tried one
thing.
"""

import itertools
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
import ppcdis

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/flagsweep3")

FIXED = ["/c", "/nologo"]

# Independent axes. None means "do not pass anything on this axis".
AXES_QUICK = [
    ["/O2", "/O1", "/Ox"],
    [None, "/Ou"],                      # prescheduling -- PowerPC only
    [None, "/Ot", "/Os"],
    ["/Gy", None],
    ["/GS-"],
    ["/fp:fast", "/fp:precise"],
]

AXES_FULL = [
    ["/O2", "/O1", "/Ox", "/Og"],
    [None, "/Ou"],
    [None, "/Ot", "/Os"],
    [None, "/Ob0", "/Ob1", "/Ob2"],
    [None, "/Oi", "/Oi-"],
    ["/Gy", None],
    ["/GS-"],
    ["/fp:fast", "/fp:precise"],
    [None, "/GR-"],
]


def compile_flags(src, flags):
    WORK.mkdir(parents=True, exist_ok=True)
    obj = WORK / "s.obj"
    if obj.exists():
        obj.unlink()
    env = {"PATH": str((XDK / "bin/win32").resolve()),
           "INCLUDE": str(INCLUDE.resolve()),
           "SystemRoot": "C:/Windows", "TEMP": str(WORK.resolve())}
    cmd = ([str(CL.resolve())] + FIXED + list(flags)
           + ["/Fo" + str(obj.resolve()), str(src.resolve())])
    r = subprocess.run(cmd, capture_output=True, text=True,
                       cwd=str(WORK.resolve()), env=env)
    if r.returncode != 0 or not obj.exists():
        return None
    fns = coff_functions(obj.read_bytes())
    if not fns:
        return None
    _sym, code, mask = max(fns, key=lambda f: len(f[1]))
    code, _mask = trim_padding(code, mask)
    return code


def score(code, tbytes, tsize):
    n = min(len(code), tsize) // 4
    same = 0
    for i in range(n):
        if (struct.unpack_from(">I", tbytes, i * 4)[0]
                == struct.unpack_from(">I", code, i * 4)[0]):
            same += 1
    return same


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    full = "--full" in argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 1
    src = Path(args[0])
    target = int(args[1], 16)

    img = Image()
    sizes = dict(load_inventory())
    if target not in sizes:
        print("%08X is not a known function start" % target)
        return 1
    tsize = sizes[target]
    tbytes = img.read(target, tsize)
    words = tsize // 4

    axes = AXES_FULL if full else AXES_QUICK
    space = [tuple(f for f in combo if f is not None)
             for combo in itertools.product(*axes)]
    # Distinct flag strings only; axes can collide once None is dropped.
    seen, combos = set(), []
    for c in space:
        if c not in seen:
            seen.add(c)
            combos.append(c)

    print("target %08X, %d byte(s), %d word(s)" % (target, tsize, words))
    print("source %s" % src)
    print("sweeping %d distinct flag combination(s)%s\n"
          % (len(combos), " (--full)" if full else ""))

    results, failed = [], 0
    for flags in combos:
        code = compile_flags(src, flags)
        if code is None:
            failed += 1
            continue
        results.append((score(code, tbytes, tsize), len(code), flags, code))

    if not results:
        print("every combination failed to compile.")
        return 2

    results.sort(key=lambda r: (r[0], -abs(r[1] - tsize)), reverse=True)
    best = results[0]

    by_score = {}
    for same, sz, flags, _code in results:
        by_score.setdefault((same, sz), []).append(flags)
    print("  %-8s %-8s %s" % ("words", "size", "flag combinations"))
    for (same, sz), fl in sorted(by_score.items(), reverse=True):
        tag = "%d/%d" % (same, words)
        szt = "%d B%s" % (sz, "" if sz == tsize else " (%+d)" % (sz - tsize))
        print("  %-8s %-8s %d combination(s), e.g. %s"
              % (tag, szt, len(fl), " ".join(fl[0]) or "(none)"))

    print("\n%d combination(s) compiled, %d failed, of %d tried"
          % (len(results), failed, len(combos)))
    print("best: %d/%d words with %s" % (best[0], words, " ".join(best[2])))

    if best[0] == words and best[1] == tsize:
        print("\nEXACT MATCH")
        return 0

    print("\ndiff for the best combination:")
    n = min(len(best[3]), tsize) // 4
    for i in range(n):
        va = target + i * 4
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", best[3], i * 4)[0]
        if a == b:
            continue
        print("  %08X  want %08x  %-30s" % (va, a, ppcdis.words([a], va)[0][2]))
        print("            got  %08x  %-30s" % (b, ppcdis.words([b], va)[0][2]))
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
