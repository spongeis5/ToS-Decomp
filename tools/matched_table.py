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
from libmatch import coff_functions, trim_padding, pick_function

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
    # ONE picker, in libmatch. This used to try the mangled name, then a
    # SUBSTRING, then fall back to every function in the object and take the
    # largest -- three guesses in a row, none of them announced. It reported
    # `vorbis_book_decode` (100 bytes) as 572, the length of
    # `vorbis_book_decodev_add`, because the first name is a prefix of the
    # second and there was no exact-match test between them. Four rows were
    # wrong and the headline byte count was 488 too high, which is how
    # build.py and report.py came to disagree.
    got, why = pick_function(coff_functions(blob), sym)
    if got is None:
        print("  %-34s %08X  %s" % (src, addr, why), file=sys.stderr)
        return None
    _n, code, mask = got
    code, _m = trim_padding(code, mask)
    return len(code)


# Files whose contents tools/gen_typeids.py and tools/gen_accessors.py wrote
# from each function's own encoding. They are real matches -- compiled and
# compared like every other -- but they are one expression each, and 708 of
# them alongside 217 hand-written functions would make the headline count say
# something false about how much of this game has been read. So they are
# summarised on their own line and kept out of the table.
from category import is_generated, is_upstream, category      # noqa: E402


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
    gen_n = 0
    gen_bytes = 0
    up_n = 0
    up_bytes = 0
    for src, addr, sym, flags in rows():
        n = compiled_size(src, sym, flags, addr)
        if n is None:
            failed.append((src, addr))
            continue
        total += n
        if is_generated(src):
            gen_n += 1
            gen_bytes += n
            continue
        # UPSTREAM is summarised rather than listed, for the same reason the
        # generated stubs are: this table is a record of what has been READ
        # off the disassembly, ranked by how much of the image calls it.
        # libvorbis's mdct_backward is 832 real bytes of the image and it was
        # obtained, not recovered; listing it beside functions someone worked
        # out from their register discipline would make the table say
        # something false about both.
        if is_upstream(src):
            up_n += 1
            up_bytes += n
            continue
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
    if gen_n:
        lines.append("| *(%d generated)* | %d | - | `vt_typeid_*`, "
                     "`vt_const_*`, `vt_acc_*` | one expression each | `/O2` |"
                     % (gen_n, gen_bytes))
    if up_n:
        lines.append("| *(%d upstream)* | %d | - | `thirdparty/ogg_vorbis/` | "
                     "libogg 1.1.3 + libvorbis 1.2.0, obtained not recovered "
                     "| `/O2` |" % (up_n, up_bytes))
    return "\n".join(lines), len(body), total, gen_n, gen_bytes, up_n, up_bytes


def main(argv):
    table, count, total, gen_n, gen_bytes, up_n, up_bytes = build_table()
    doc = DOC.read_text(encoding="utf-8")
    i = doc.index(HEADER)
    j = doc.index("\n---", i)
    new = doc[:i] + table + doc[j:]

    # Two separate substitutions, each anchored to as little text as it can
    # be. The first version of this matched `[^\n]*` after the bold total and
    # ate the rest of the sentence -- "Verify all of them, plus the
    # reconstructing" -- leaving a paragraph that began "build and five
    # negative controls". A regex that rewrites documentation should replace
    # exactly the token it maintains and nothing adjacent to it.
    import re
    new = re.sub(r"\*\*\d+ functions, \d+ bytes\.\*\*",
                 "**%d functions, %d bytes.**"
                 % (count + gen_n + up_n, total), new, count=1)
    # The headline count alone would be true and misleading in one breath, so
    # the split is maintained beside it rather than left to drift by hand.
    # Three ways now: upstream code reproduces the image exactly and says
    # nothing about how much of the game has been read.
    new = re.sub(r"SPLIT: \d+ hand-written, \d+ bytes; \d+ generated, "
                 r"\d+ bytes(?:; \d+ upstream, \d+ bytes)?\.",
                 "SPLIT: %d hand-written, %d bytes; %d generated, %d bytes; "
                 "%d upstream, %d bytes."
                 % (count, total - gen_bytes - up_bytes, gen_n, gen_bytes,
                    up_n, up_bytes),
                 new, count=1)
    n_os = sum(1 for r in rows() if r[3] and "/Os" in r[3])
    new = re.sub(r"\*\*The retail build did NOT use one optimisation level "
                 r"everywhere\.\*\* \d+ of",
                 "**The retail build did NOT use one optimisation level "
                 "everywhere.** %d of" % n_os, new, count=1)

    if "--check" in argv:
        same = (new == doc)
        print("MATCHED.md is %s (%d hand-written + %d generated + %d upstream "
              "= %d function(s), %d bytes)"
              % ("up to date" if same else "STALE -- run without --check",
                 count, gen_n, up_n, count + gen_n + up_n, total))
        return 0 if same else 1

    DOC.write_text(new, encoding="utf-8")
    print("MATCHED.md: %d hand-written (%d bytes) + %d generated (%d bytes)"
          % (count, total - gen_bytes - up_bytes, gen_n, gen_bytes))
    print("            + %d upstream (%d bytes)" % (up_n, up_bytes))
    print("            = %d function(s), %d bytes, %d at /O2 /Os"
          % (count + gen_n + up_n, total, n_os))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
