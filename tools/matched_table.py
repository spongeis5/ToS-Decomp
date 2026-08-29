"""Regenerate the function table in MATCHED.md from src/manifest.txt.

    python tools/matched_table.py            rewrite the table
    python tools/matched_table.py --check    fail if it is out of date

The table used to be maintained by hand, which is the same drift that let
verify.py carry its own copy of the match list and fall out of step with
build.py. `src/manifest.txt` is the one source of truth: it is what
`build.py` compiles and what `verify.py` checks, so anything derived from it
should be derived, not retyped.

Sizes come from OUR code, not from the inventory, because the inventory is
wrong in both directions -- short where a tail call's dead `blr` was not
counted, long where one `.pdata` unwind row covers several frameless bodies.
The compiled length is the only figure that is a fact about the function.

The table is delimited by its header row and the `---` that follows it;
everything else in MATCHED.md is prose and is left alone.
"""

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from libmatch import coff_functions, trim_padding

import xdkcc

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "MATCHED.md"
MANIFEST = ROOT / "src/manifest.txt"
HEADER = "| address | bytes | callers | source | symbol | flags |"


def rows():
    out = []
    for line in MANIFEST.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split()
        sym, flags = None, None
        for extra in f[2:]:
            if extra.startswith("flags="):
                flags = extra[len("flags="):]
            elif extra != "-":
                sym = extra
        out.append((f[0], int(f[1], 16), sym, flags))
    return out


def callers():
    counts = {}
    p = ROOT / "build/callgraph.txt"
    if not p.exists():
        return None
    for line in p.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) == 2:
            try:
                k = int(f[1], 16)
            except ValueError:
                continue
            counts[k] = counts.get(k, 0) + 1
    return counts


def compiled_size(src, sym, flags, addr):
    use = ["/c", "/nologo"] + (flags.split(",") if flags
                               else ["/O2", "/Gy", "/GS-", "/fp:fast"])
    work = ROOT / "build/table"
    work.mkdir(parents=True, exist_ok=True)
    # NOT the symbol: a mangled C++ name contains `?` and `@`, which are
    # illegal in a Windows filename, so a constructor made compile_obj fail
    # and this refused to write a table -- correctly, but for a reason that
    # had nothing to do with the source. The address is unique and safe.
    tag = "%s_%08X" % (Path(src).stem, addr)
    blob, err = xdkcc.compile_obj(str(ROOT / src), work / (tag + ".obj"),
                                  use, work)
    if blob is None:
        return None
    fns = coff_functions(blob)
    if sym:
        picked = [f for f in fns if ("?" + sym + "@@") in f[0]] \
            or [f for f in fns if sym in f[0]]
        fns = picked or fns
    if not fns:
        return None
    _n, code, mask = max(fns, key=lambda f: len(f[1]))
    code, _m = trim_padding(code, mask)
    return len(code)


def build_table():
    cg = callers()
    if cg is None:
        print("build/callgraph.txt is missing -- run tools/discover.py.")
        print("Refusing to write a table with a caller count of zero for")
        print("every row, because that reads as a fact rather than a gap.")
        sys.exit(1)
    body = []
    total = 0
    failed = []
    for src, addr, sym, flags in rows():
        n = compiled_size(src, sym, flags, addr)
        if n is None:
            failed.append((src, addr))
            continue
        total += n
        body.append((cg.get(addr, 0), addr, n, src, sym, flags))
    if failed:
        print("%d source(s) would not compile; refusing to write a table that"
              % len(failed))
        print("silently omits them:")
        for src, addr in failed:
            print("    %-32s %08X" % (src, addr))
        sys.exit(1)
    body.sort(key=lambda r: (-r[0], r[1]))

    lines = [HEADER, "|---|---|---|---|---|---|"]
    for c, addr, n, src, sym, flags in body:
        lvl = "/O2 /Os" if (flags and "/Os" in flags) else "/O2"
        lines.append("| `%08X` | %d | %d | `%s` | %s | `%s` |"
                     % (addr, n, c, Path(src).name, sym or "-", lvl))
    return "\n".join(lines), len(body), total


def main(argv):
    table, count, total = build_table()
    doc = DOC.read_text(encoding="utf-8")
    i = doc.index(HEADER)
    j = doc.index("\n---", i)
    new = doc[:i] + table + doc[j:]

    # The two counts in the prose above the table are part of the same fact.
    import re
    new = re.sub(r"\*\*\d+ functions, \d+ bytes\.\*\*",
                 "**%d functions, %d bytes.**" % (count, total), new, count=1)
    n_os = sum(1 for r in rows() if r[3] and "/Os" in r[3])
    new = re.sub(r"\*\*The retail build did NOT use one optimisation level "
                 r"everywhere\.\*\* \d+ of",
                 "**The retail build did NOT use one optimisation level "
                 "everywhere.** %d of" % n_os, new, count=1)

    if "--check" in argv:
        same = (new == doc)
        print("MATCHED.md is %s (%d function(s), %d bytes)"
              % ("up to date" if same else "STALE -- run without --check",
                 count, total))
        return 0 if same else 1

    DOC.write_text(new, encoding="utf-8")
    print("MATCHED.md: %d function(s), %d bytes, %d at /O2 /Os"
          % (count, total, n_os))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
