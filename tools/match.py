"""The matching loop: compile a candidate, compare it against the retail bytes.

    python tools/match.py src/grid_indices.cpp 822607F0

Compiles with the XDK's own cl.exe (15.00.8153), pulls the code the compiler
emitted, and diffs it instruction by instruction against the image.

Notes that cost time to learn:

  * cl.exe is invoked through subprocess directly, NOT through a shell.  Git
    Bash rewrites MSVC-style `/c` and `/nologo` into Windows paths and the
    flags are silently dropped.
  * A COMDAT section is padded; the trailing nops/zeros are trimmed before
    comparison, and what was trimmed is reported.
  * Relocated words are marked.  An object refers to symbols by placeholder,
    so a difference in a relocated word is EXPECTED and is shown separately
    from a real mismatch -- counting them together would make a correct
    function look wrong.

Exit status is 0 only on an exact match of the non-relocated bytes.
"""

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"

DEFAULT_FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]

import ppcdis


def text(word, va):
    # One decoder for both sides. Capstone cannot read VMX128, and a diff
    # where one side shows `.long` is not a comparison of instructions.
    return ppcdis.words([word], va)[0][2]


def compile_one(src, flags, workdir):
    workdir.mkdir(parents=True, exist_ok=True)
    obj = workdir / (src.stem + ".obj")
    if obj.exists():
        obj.unlink()
    env = {"PATH": str((XDK / "bin/win32").resolve()),
           "INCLUDE": str(INCLUDE.resolve()),
           "SystemRoot": "C:\\Windows", "TEMP": str(workdir.resolve())}
    cmd = [str(CL.resolve())] + flags + ["/Fo" + str(obj.resolve()),
                                         str(src.resolve())]
    r = subprocess.run(cmd, capture_output=True, text=True,
                       cwd=str(workdir.resolve()), env=env)
    if r.returncode != 0 or not obj.exists():
        print("COMPILE FAILED (exit %d)" % r.returncode)
        print(r.stdout)
        print(r.stderr)
        return None
    for line in r.stdout.splitlines():
        s = line.strip()
        if s and not s.endswith(".cpp") and not s.endswith(".c"):
            print("  cl: %s" % s)
    return obj


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    src = Path(argv[1])
    target = int(argv[2], 16)
    flags = list(DEFAULT_FLAGS)
    if "--flags" in argv:
        flags = argv[argv.index("--flags") + 1].split()
    sym_want = argv[argv.index("--sym") + 1] if "--sym" in argv else None

    img = Image()
    sizes = dict(load_inventory())
    if target not in sizes:
        print("%08X is not a known function start." % target)
        print("  The inventory holds %d function(s). If build/functions_all.txt"
              % len(sizes))
        print("  is missing or stale, run tools/inventory.py.")
        return 1
    tsize = sizes[target]
    tbytes = img.read(target, tsize)

    work = Path("build/match")
    obj = compile_one(src, flags, work)
    if obj is None:
        return 2

    fns = coff_functions(obj.read_bytes())
    if sym_want:
        fns = [f for f in fns if sym_want in f[0]]
    if not fns:
        print("no PowerPC function found in the object")
        return 2
    if len(fns) > 1:
        print("%d functions in the object; using the largest. "
              "Use --sym to pick one:" % len(fns))
        for n, c, _m in fns:
            print("    %-50s %d byte(s)" % (n, len(c)))
    sym, code, mask = max(fns, key=lambda f: len(f[1]))
    code, mask = trim_padding(code, mask)

    print()
    print("target  %08X  %d byte(s)" % (target, tsize))
    print("ours    %-40s %d byte(s)%s"
          % (sym[:40], len(code),
             "" if len(code) == tsize else "   <-- SIZE DIFFERS"))
    print()

    n = min(len(code), tsize) // 4
    same = diff = reloc_diff = 0
    for i in range(n):
        va = target + i * 4
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", code, i * 4)[0]
        relocated = not all(mask[i * 4 : i * 4 + 4])
        if a == b:
            same += 1
            continue
        if relocated:
            reloc_diff += 1
            flag = "r"
        else:
            diff += 1
            flag = "X"
        print(" %s %08X  want %08x  %-34s" % (flag, va, a, text(a, va)))
        print("            got  %08x  %-34s" % (b, text(b, va)))

    extra = abs(len(code) - tsize) // 4
    print()
    print("%d word(s) compared: %d identical, %d differ, %d differ in a "
          "relocated word (expected)" % (n, same, diff, reloc_diff))
    if extra:
        print("%d word(s) of length difference not compared" % extra)

    if diff == 0 and len(code) == tsize:
        print("\nMATCH: every non-relocated word is identical.")
        return 0
    print("\nNO MATCH.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
