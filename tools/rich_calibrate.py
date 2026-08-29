"""Measure what each Rich-header product id MEANS, using this XDK.

The retail image's Rich header is a census of the tools that produced its
objects:

    prodid 131 build 8153 count  359
    prodid 132 build 8153 count 1090
    prodid 138 build 8153 count   54
    prodid 145/146/147/149 ...

The community prodid table is deliberately NOT consulted. We have the exact
toolchain that produced those rows, so the meaning of each id is measurable:
build with a known flag set, read the id that appears. A measured id cannot be
the wrong id from a table for the wrong compiler generation.

    python tools/rich_calibrate.py [arm ...]

Each arm compiles one or more translation units and links them, then decodes
the Rich header of the intermediate PE the Xbox 360 linker writes beside the
XEX. The CONTROL arm ("cpp") must produce prodid 132 -- the id that dominates
the retail image. If it does not, the harness is measuring something other
than what the retail linker did and every other row is uninterpretable, so
the script refuses to print conclusions.
"""

import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import rich

ROOT = Path(__file__).resolve().parent.parent
XDK = ROOT / "SDKFiles/xdk/XDK"
BIN = XDK / "bin/win32"
CL = BIN / "cl.exe"
LINK = BIN / "link.exe"
WORK = ROOT / "build/rich_cal"

# One function, valid as both C and C++, exported so nothing is dead-stripped.
UNIT = """
__declspec(dllexport) int %(name)s(int a, int b)
{
    int t = a * 3 + b;
    return t ^ (t >> 5);
}
"""

# name -> (list of (tu_index, extra cl flags), extra link flags, why)
ARMS = {
    "cpp":       ([(0, [])],                       [],
                  "CONTROL: one C++ TU, no /GL"),
    "c":         ([(0, ["/TC"])],                  [],
                  "one C TU (/TC), no /GL -- does C get its own id?"),
    "cpp_gl":    ([(0, ["/GL"])],                  ["/LTCG"],
                  "one C++ TU with /GL, linked /LTCG"),
    "c_gl":      ([(0, ["/TC", "/GL"])],           ["/LTCG"],
                  "one C TU with /GL, linked /LTCG"),
    "gl_nolink": ([(0, ["/GL"])],                  [],
                  "/GL object, linker NOT told /LTCG"),
    "mixed":     ([(0, []), (1, ["/GL"])],         ["/LTCG"],
                  "THE ONE THAT MATTERS: plain TU + /GL TU, linked /LTCG"),
    "pgi":       ([(0, ["/GL"])],                  ["/LTCG:PGI"],
                  "PGO instrumented build"),
    "noexport":  ([(0, [])],                       [],
                  "no dllexport -- which id disappears?"),
}


def env():
    return {
        "PATH": str(BIN),
        "INCLUDE": str((XDK / "include/xbox").resolve()),
        "LIB": str((XDK / "lib/xbox").resolve()),
        "SystemRoot": "C:/Windows",
        "TEMP": str(WORK.resolve()),
    }


def run(args, cwd):
    r = subprocess.run([str(a) for a in args], capture_output=True, text=True,
                       cwd=str(cwd), env=env())
    return r.returncode, (r.stdout + r.stderr).strip()


def build_arm(name, spec):
    tus, linkflags, _why = spec
    d = WORK / name
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)

    objs = []
    for idx, clflags in tus:
        fn = "f%d" % idx
        src = "u%d.cpp" % idx
        body = UNIT % {"name": fn}
        if name == "noexport":
            body = body.replace("__declspec(dllexport) ", "")
        (d / src).write_text(body)
        obj = "u%d.obj" % idx
        rc, out = run([CL.resolve(), "/c", "/nologo", "/O2", "/Gy", "/GS-",
                       "/fp:fast"] + clflags + ["/Fo" + obj, src], d)
        if rc != 0 or not (d / obj).exists():
            return None, "compile failed: " + out.splitlines()[0] if out else "compile failed"
        objs.append(obj)

    rc, out = run([LINK.resolve(), "/nologo", "/DLL", "/OUT:t.dll"]
                  + linkflags + objs, d)
    pe = d / "t.pe"
    if rc != 0 or not pe.exists():
        # Report the diagnostic lines, not line 1 -- link.exe prints
        # "Creating library ..." first and that is not the failure.
        bad = [ln for ln in out.splitlines()
               if "error" in ln.lower() or "warning" in ln.lower()]
        return None, "link rc=%d: %s" % (rc, " | ".join(bad) or out or "(silent)")
    try:
        key, rows = rich.decode(pe.read_bytes())
    except rich.NoRichHeader as e:
        return None, "no rich header: %s" % e
    return rows, None


def main(argv):
    want = argv[1:] if len(argv) > 1 else list(ARMS)
    for a in want:
        if a not in ARMS:
            print("unknown arm %r; known: %s" % (a, " ".join(ARMS)))
            return 1
    WORK.mkdir(parents=True, exist_ok=True)

    results = {}
    for name in want:
        rows, err = build_arm(name, ARMS[name])
        results[name] = (rows, err)
        why = ARMS[name][2]
        print("== %-10s %s" % (name, why))
        if rows is None:
            print("   FAILED: %s" % err)
            continue
        for prodid, build, count in rows:
            print("   prodid %-4d build %-6d count %d" % (prodid, build, count))
        print("")

    ctl = results.get("cpp", (None, None))[0]
    if ctl is None:
        print("CONTROL ARM DID NOT BUILD -- nothing here is interpretable.")
        return 2
    ctl_ids = set(p for p, _b, _c in ctl)
    if 132 not in ctl_ids:
        print("CONTROL ARM produced ids %s, which does not include 132 -- the id"
              % sorted(ctl_ids))
        print("that dominates the retail image. This harness is not reproducing")
        print("what the retail linker did; refusing to interpret the other arms.")
        return 2

    print("control arm carries prodid 132, as the retail image's bulk does.")
    print("")
    print("%-10s  %s" % ("arm", "prodids"))
    for name in want:
        rows, err = results[name]
        if rows is None:
            print("%-10s  FAILED (%s)" % (name, err))
        else:
            print("%-10s  %s" % (name, " ".join(
                "%d(x%d)" % (p, c) for p, _b, c in rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
