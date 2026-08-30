"""Regenerate HANDBOOK.md's tool inventory from the tools themselves.

    python tools/tool_table.py            rewrite the table
    python tools/tool_table.py --check    fail if it has drifted

The table was maintained by hand and rotted exactly the way the README's
figures did before `readme_stats.py`: **27 of 77 tools were missing from
it**, including `sweep.py` and `xdkcc.py`, which are in the daily loop. A
reader looking for a tool that is not listed concludes it does not exist.

So it is derived, like MATCHED.md's table and the figures block. The
description of each tool is THE FIRST LINE OF ITS OWN DOCSTRING, which is the
one place that cannot drift from the tool -- it is read by anyone running
`python tools/<name>.py` with no arguments, so it is already maintained.

Two things stay curated, because generation should not throw away judgement:

  ORDER, which follows the pipeline rather than the alphabet -- unpack, map,
  discover, analyse, match, build, verify. A tool not named there is appended
  at the end and reported, so a new tool is listed the moment it exists and
  can be moved into place later.

  EMPHASIS, on the handful a newcomer should find first.

Completeness is not a judgement call and is not curated: the table is built by
enumerating `tools/*.py`, so a tool cannot be absent from it.
"""

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "HANDBOOK.md"
TOOLS = ROOT / "tools"
HEADER = "| tool | what it does |"

# The pipeline order the hand-written table had. Anything not here is appended
# alphabetically and flagged, which is the signal to place it deliberately.
ORDER = [
    "xex.py", "flatten_pe.py", "verify_mapping.py", "pdata.py",
    "inventory.py", "peimage.py", "ppcdis.py", "disasm.py",
    "discover.py", "addrtaken.py", "switches.py", "truncated.py",
    "interior.py", "rtti.py", "vtables.py", "xeximports.py", "srcfiles.py",
    "profnames.py", "strings.py", "xref.py", "tunits.py", "segment.py",
    "attribute.py", "candidates.py", "libmatch.py", "oggmatch.py",
    "ogg_rows.py", "compid.py", "objcode.py", "coffreloc.py", "coffwrite.py",
    "xdkcc.py", "match.py", "batch.py", "bridge.py", "climb.py", "layout.py",
    "serial.py", "sweep.py", "prune_attempts.py", "permute.py",
    "permuter.py", "flagsweep.py", "flagpairs.py", "gen_typeids.py",
    "gen_accessors.py", "category.py", "build.py", "link.py",
    "matched_table.py", "readme_stats.py", "tool_table.py", "open_stalls.py",
    "coverage.py",
    "objdiff_export.py", "report.py", "publish_report.py", "dashdata.py",
    "dashhistory.py", "dashboard.py", "dumptext.py", "verify.py",
    "test_shrink.py", "test_coffreloc.py", "test_permute.py",
    "test_mutations.py", "test_lock.py", "test_xdkcc_cache.py",
    "test_privacy.py", "test_privacy_guard.py", "test_doc_guards.py",
    "test_verify_honesty.py",
    "vmx128_check.py", "vmx128_oracle.py", "vmx128_table.py",
    "vmx128_intrinsics.py", "rich.py", "rich_calibrate.py",
    "pemanifest.py", "fix_manifests.py", "verify_ghidra.py",
]

# The handful a newcomer should find first. The text replaces the docstring
# line; everything else is generated.
EMPHASIS = {
    "verify.py": "**run everything**, and say which checks can actually fail",
    "match.py": "**the matching loop**: compile a candidate, diff against the image",
    "build.py": "**the reconstructing build**: compile, resolve relocations, hash `.text`",
    "link.py": "**the real link**: `link.exe` places a contiguous run at its retail address",
    "ppcdis.py": "disassembly via binutils — **the only decoder here that knows VMX128**",
    "verify_mapping.py": "**decide** the VA→offset mapping against `.pdata`, both arms scored",
    "sweep.py": "**recover work** whose manifest row was never written; `--attempts` scores near-misses",
    "bridge.py": "**which unmatched function would MERGE two linked runs** — ranked by the span it unlocks",
    "verify_ghidra.py": "**superseded**; kept as a worked example of a vacuous check",
}


def summary(path):
    """-> (first docstring line, None) or (None, why not).

    A SYNTAX ERROR AND A MISSING DOCSTRING ARE NOT THE SAME THING, and the
    first version of this returned None for both. It reported
    `vmx128_intrinsics.py` as "no docstring" when the file did not PARSE:
    five `\\n` escapes inside one string had been written as literal
    newlines -- the shell trap SHELL-TRAPS.md documents -- so the tool could
    not run at all and had not been able to for some time. Nothing else
    noticed, because nothing imports it.

    A benign-sounding cause reported for a serious one is the failure mode
    this repository exists to avoid, so the two are separated and the caller
    prints them apart.
    """
    try:
        tree = ast.parse(path.read_text(encoding="utf-8", errors="ignore"))
    except SyntaxError as e:
        return None, "DOES NOT PARSE: line %s, %s" % (e.lineno, e.msg)
    doc = ast.get_docstring(tree)
    if not doc:
        return None, "no module docstring"
    first = doc.strip().splitlines()[0].strip()
    return (first, None) if first else (None, "empty docstring first line")


def build():
    """-> (table text, [(tool, why)], [tools not in ORDER])."""
    names = sorted(p.name for p in TOOLS.glob("*.py"))
    undocumented, unordered = [], []
    ordered = [n for n in ORDER if n in names]
    for n in names:
        if n not in ordered:
            ordered.append(n)
            unordered.append(n)

    lines = [HEADER, "|---|---|"]
    for n in ordered:
        text = EMPHASIS.get(n)
        if text is None:
            text, why = summary(TOOLS / n)
            if text is None:
                undocumented.append((n, why))
                text = "_(%s)_" % why
            # A table cell cannot hold a pipe, and a trailing full stop reads
            # oddly in a column of fragments.
            text = text.replace("|", "\\|").rstrip(".")
        lines.append("| `%s` | %s |" % (n, text))
    return "\n".join(lines), undocumented, unordered


def main(argv):
    table, undocumented, unordered = build()
    doc = DOC.read_text(encoding="utf-8")
    if HEADER not in doc:
        print("HANDBOOK.md has no tool table header (%r)" % HEADER)
        return 1
    i = doc.index(HEADER)
    j = doc.index("\n---\n", i)
    new = doc[:i] + table + doc[j:]

    n = table.count("\n") - 1
    broken = [(t, w) for t, w in undocumented if w.startswith("DOES NOT")]
    if broken:
        print("%d TOOL(S) DO NOT PARSE. They cannot run, and nothing else in"
              % len(broken))
        print("this repository would notice, because nothing imports them:")
        for t, w in broken:
            print("    %-26s %s" % (t, w))
        print("")
    plain = [(t, w) for t, w in undocumented if not w.startswith("DOES NOT")]
    if plain:
        print("%d tool(s) cannot be described from their own source:"
              % len(plain))
        for t, w in plain:
            print("    %-26s %s" % (t, w))
        print("")
    if unordered:
        print("%d tool(s) are not in ORDER and were appended at the end;"
              " move them into the pipeline when you know where they go:"
              % len(unordered))
        for t in unordered:
            print("    %s" % t)
        print("")

    if "--check" in argv:
        same = (new == doc) and not undocumented
        print("HANDBOOK.md tool table is %s (%d tool(s))"
              % ("up to date" if same else "STALE -- run without --check", n))
        return 0 if same else 1

    DOC.write_text(new, encoding="utf-8")
    print("HANDBOOK.md tool table: %d tool(s)" % n)
    return 1 if undocumented else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
