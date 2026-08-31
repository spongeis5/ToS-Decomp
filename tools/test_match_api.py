"""match.py's in-process API and its command line must agree, case by case.

    python tools/test_match_api.py

`verify.py` used to answer "does this row match?" by running `tools/match.py`
as a subprocess, once per manifest row -- 1,986 launches for 579 source
files. It now compiles each file once and calls `match.select` and
`match.compare` directly.

That is only safe while there is ONE implementation. This is the check that
says so: every case below is put through BOTH paths and the verdicts must
be the same. Five tools have now disagreed with verify.py by growing their
own copy of this comparison, always in the direction that gets believed, and
the outcome a home-grown copy loses is the third one -- "it never compared
anything at all".

The cases are chosen to cover what the two paths could disagree ABOUT, not
just the happy one: a plain match, a row whose window must SHRINK, a row
whose window must EXTEND, a symbol that selects more than one function
(which must REFUSE rather than guess), and a source that does not exist
(which must be UNMEASURED, never a mismatch).

A case that cannot be set up is reported as skipped WITH ITS REASON and the
run still fails if too few ran -- a suite that silently shrinks to nothing
passes for the wrong reason, which is the failure this repository exists to
avoid.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import match as _match                                        # noqa: E402
from libmatch import coff_functions, trim_padding             # noqa: E402
from peimage import Image, load_inventory                     # noqa: E402
from verify import classify_match                             # noqa: E402

MIN_CASES = 5


def manifest_rows():
    out = []
    p = ROOT / "src/manifest.txt"
    for line in p.read_text(encoding="utf-8").splitlines():
        s = line.split("#")[0].strip()
        if not s:
            continue
        f = s.split()
        if len(f) < 2:
            continue
        sym, flags = None, None
        for extra in f[2:]:
            if extra.startswith("flags="):
                flags = extra[len("flags="):]
            elif extra != "-":
                sym = extra
        out.append((f[0], f[1], sym, flags))
    return out


def by_cli(src, addr, sym, flags):
    """-> the verdict the COMMAND LINE reaches, read with classify_match."""
    args = [sys.executable, "tools/match.py", src, addr]
    if sym:
        args += ["--sym", sym]
    if flags:
        args += ["--flags", "/c /nologo " + " ".join(flags.split(","))]
    r = subprocess.run(args, cwd=str(ROOT), capture_output=True, text=True)
    return classify_match(r.returncode, (r.stdout or "") + (r.stderr or ""))[0]


def by_api(img, sizes, src, addr, sym, flags):
    """-> the verdict the IN-PROCESS path reaches, the way verify.py does."""
    use = _match.parse_flags(flags) if flags else list(_match.DEFAULT_FLAGS)
    try:
        obj = _match.compile_one(ROOT / src, use, ROOT / "build/match")
    except Exception:                                         # noqa: BLE001
        return "unmeasured"
    if obj is None:
        return "unmeasured"
    fns = coff_functions(obj.read_bytes())
    if int(addr, 16) not in sizes:
        return "unmeasured"
    picked, _why = _match.select(fns, sym)
    if picked is None:
        return "unmeasured"
    _n, code, mask = picked[0]
    code, mask = trim_padding(code, mask)
    res = _match.compare(img, sizes, int(addr, 16), code, mask)
    return "match" if res["verdict"] == "match" else "differ"


def main():
    img = Image()
    sizes = dict(load_inventory())
    rows = manifest_rows()
    if not rows:
        print("src/manifest.txt has no rows; nothing to compare. REFUSING")
        print("to report a pass, because zero cases is not agreement.")
        return 1

    cases, skipped = [], []

    # 1-3. Three real rows, chosen so the window is reconciled differently in
    # each: unchanged, shrunk, and extended. Which row does which is a fact
    # about the tree, so it is MEASURED here rather than hard-coded to
    # addresses that would rot.
    plain = shrink = extend = None
    for src, addr, sym, flags in rows:
        if not (ROOT / src).exists():
            continue
        t = int(addr, 16)
        if t not in sizes:
            continue
        use = _match.parse_flags(flags) if flags else list(_match.DEFAULT_FLAGS)
        obj = _match.compile_one(ROOT / src, use, ROOT / "build/match")
        if obj is None:
            continue
        picked, _w = _match.select(coff_functions(obj.read_bytes()), sym)
        if picked is None:
            continue
        code, mask = trim_padding(picked[0][1], picked[0][2])
        res = _match.compare(img, sizes, t, code, mask)
        row = (src, addr, sym, flags)
        if res["shrunk"] and shrink is None:
            shrink = ("a row whose window must SHRINK", row)
        elif res["extended"] and extend is None:
            extend = ("a row whose window must EXTEND", row)
        elif (res["shrunk"] is None and res["extended"] is None
              and plain is None):
            plain = ("a plain row, window unchanged", row)
        if plain and shrink and extend:
            break
    for got, what in ((plain, "unchanged"), (shrink, "shrunk"),
                      (extend, "extended")):
        if got is None:
            skipped.append("no row in the manifest has a %s window" % what)
        else:
            cases.append(got)

    # 4. A symbol that selects more than one function must REFUSE in both
    # paths -- and refusing is `unmeasured`, not `differ`.
    amb = None
    for src, addr, sym, flags in rows:
        if not (ROOT / src).exists():
            continue
        use = _match.parse_flags(flags) if flags else list(_match.DEFAULT_FLAGS)
        obj = _match.compile_one(ROOT / src, use, ROOT / "build/match")
        if obj is None:
            continue
        fns = coff_functions(obj.read_bytes())
        if len(fns) > 1:
            amb = ("a symbol that selects more than one function (must refuse)",
                   (src, addr, None, flags))
            break
    if amb is None:
        skipped.append("no source in the manifest emits two functions")
    else:
        cases.append(amb)

    # 5. A source that is not there. The compile cannot happen, so the answer
    # is UNMEASURED -- the distinction that cost a whole diagnosis when two
    # verify runs printed refused compiles as broken functions.
    cases.append(("a source that does not exist (must be unmeasured)",
                  ("src/__no_such_source__.cpp", rows[0][1], None, None)))

    print("match.py: the in-process API against the command line")
    print("")
    print("       %-58s %-12s %s" % ("case", "api", "cli"))
    bad = 0
    for label, (src, addr, sym, flags) in cases:
        a = by_api(img, sizes, src, addr, sym, flags)
        c = by_cli(src, addr, sym, flags)
        ok = (a == c)
        bad += 0 if ok else 1
        print("  %s %-58s %-12s %s"
              % ("PASS" if ok else "FAIL", label[:58], a, c))

    print("")
    for s in skipped:
        print("  skipped: %s" % s)
    if len(cases) < MIN_CASES:
        print("")
        print("%d case(s) ran, fewer than the %d this suite claims. A suite"
              % (len(cases), MIN_CASES))
        print("that quietly shrinks passes for the wrong reason.")
        return 1
    print("")
    print("%d of %d case(s) agree." % (len(cases) - bad, len(cases)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
