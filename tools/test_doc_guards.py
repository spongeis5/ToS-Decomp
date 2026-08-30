"""The documentation guards must FAIL on what they claim to catch.

    python tools/test_doc_guards.py

`readme_stats.py --check`, `matched_table.py --check`, `tool_table.py --check`
and `prune_attempts.py --check` all report "up to date" on a clean tree. That
tells you nothing on its own -- a check that cannot fail reports success for
the same reason a working one does, and this repository has deleted two such
checks already.

So each is handed the violation it exists to catch, and required to refuse.

ONE OF THEM WAS ALREADY VACUOUS when this was written. `readme_stats.py`
substituted the front page's headline only `if FRONT_RE.search(front)`, and
left the text untouched otherwise -- so editing that sentence, or deleting it,
made `--check` compare the file to itself and report success. The check that
exists *because the front page was once wrong by six times* could be silenced
by removing the sentence it maintains. It fails on a missing headline now, and
the control below is what found it.

Everything is restored byte for byte in a `finally`, and the last check is
that the tree is exactly as it was found -- because a test that plants
violations has to prove it took them all back out.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RESULTS = []


def run(args):
    r = subprocess.run([sys.executable] + args, cwd=str(ROOT),
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def clean_passes(name, cmd):
    rc, _out = run(cmd)
    check(name, rc == 0, "" if rc == 0 else "fails on a CLEAN tree")


def control(name, path, mutate, cmd, want_in_output=None):
    """Corrupt one fact, require `cmd` to refuse, restore the bytes.

    A PLANT THAT CHANGES NOTHING IS ITS OWN FAILURE, and it is reported as
    one rather than as "NOT CAUGHT". Two controls here planted a literal
    old figure -- "124 of 180 runs" -- and the link moved to 178, so
    `bytes.replace` matched nothing, the file was rewritten identically, and
    the guard was blamed for missing a violation that was never made.

    That is the vacuous-check pattern one level up: this file exists to
    prove guards can fail, and two of its own controls had quietly stopped
    being able to. So the mutation is checked before the guard is.
    """
    p = ROOT / path
    orig = p.read_bytes()
    planted = mutate(orig)
    if planted == orig:
        check(name, False,
              "THE PLANT DID NOTHING -- this control tests nothing. The text "
              "it edits has moved; fix the control, not the guard")
        return
    try:
        p.write_bytes(planted)
        rc, out = run(cmd)
        ok = rc != 0
        if ok and want_in_output:
            ok = want_in_output in out
    finally:
        p.write_bytes(orig)
    check(name, ok, "" if ok else "NOT CAUGHT")


def first_manifest_address():
    for line in (ROOT / "src/manifest.txt").read_text(
            encoding="utf-8").splitlines():
        s = line.split("#")[0].strip()
        if s and len(s.split()) >= 2:
            return s.split()[1]
    return None


def nl_of(b):
    return b"\r\n" if b"\r\n" in b else b"\n"


def _bump(pattern):
    """A mutation that adds 1 to the first capture group of `pattern`.

    Derived rather than literal, so the control cannot go vacuous when the
    figure it perturbs legitimately changes. If the pattern matches nothing
    the returned bytes are unchanged, and `control` reports THAT as the
    failure -- which is the signal that the sentence moved.
    """
    import re as _re

    def mutate(b):
        m = _re.search(pattern, b)
        if not m:
            return b
        n = int(m.group(1))
        return (b[:m.start(1)] + str(n + 1).encode() + b[m.end(1):])
    return mutate


def _blank_open_stalls(b):
    """Empty the open-stall region, keeping both of its headings."""
    begin = b"**Still genuinely open, and the honest reasons:**"
    end = b"**Larger, in rough order of value:**"
    i = b.index(begin) + len(begin)
    j = b.index(end, i)
    return b[:i] + nl_of(b) + nl_of(b) + b[j:]


def main():
    print("documentation guards -- each must refuse what it claims to catch")
    print("")
    before = {p: (ROOT / p).read_bytes()
              for p in ("README.md", "HANDBOOK.md", "MATCHED.md",
                        "src/manifest.txt", "src/attempts.txt",
                        "src/w4_tail_floats.cpp")}

    clean_passes("tool table passes on a clean tree",
                 ["tools/tool_table.py", "--check"])
    clean_passes("MATCHED.md passes on a clean tree",
                 ["tools/matched_table.py", "--check"])
    clean_passes("README figures pass on a clean tree",
                 ["tools/readme_stats.py", "--check"])
    clean_passes("no matched address is also a near-miss",
                 ["tools/prune_attempts.py", "--check"])
    clean_passes("open-stall list agrees with the manifest",
                 ["tools/open_stalls.py", "--check"])
    clean_passes("no finished source is missing its row",
                 ["tools/sweep.py", "--check"])

    # A NEW TOOL must make the inventory stale. The table is built by
    # enumerating tools/*.py, so this is the case it cannot miss -- and the
    # one that matters, since the inventory reached 27 of 77 missing.
    probe = ROOT / "tools/zzz_probe_tool.py"
    try:
        probe.write_text('"""A probe tool, written by test_doc_guards."""\n',
                         encoding="utf-8")
        rc, _o = run(["tools/tool_table.py", "--check"])
        check("a NEW tool makes the table stale", rc != 0)
    finally:
        if probe.exists():
            probe.unlink()

    # A tool that DOES NOT PARSE must be named as such, not reported as
    # merely undocumented. vmx128_intrinsics.py was unrunnable for some time
    # and this tool called it "no docstring" -- a benign cause given for a
    # serious one.
    broken = ROOT / "tools/zzz_broken_tool.py"
    try:
        broken.write_bytes(b'"""Unterminated.\n\nx = "oops\n')
        rc, out = run(["tools/tool_table.py", "--check"])
        check("a tool that DOES NOT PARSE is named as such",
              rc != 0 and "DOES NOT PARSE" in out)
    finally:
        if broken.exists():
            broken.unlink()

    control("a changed manifest makes MATCHED.md stale",
            "src/manifest.txt",
            lambda b: b + nl_of(b) + b"src/zzz_probe.cpp 82540728 StrLen",
            ["tools/matched_table.py", "--check"])

    # THE ONE THAT WAS VACUOUS. Breaking the headline must fail, not pass.
    control("an EDITED headline makes the figures stale",
            "README.md",
            lambda b: b.replace(b"functions matched**",
                                b"functions matched?**", 1),
            ["tools/readme_stats.py", "--check"])
    control("a DELETED headline is a failure, not a no-op",
            "README.md",
            lambda b: b"\n".join(
                l for l in b.split(b"\n")
                if b"functions matched**" not in l),
            ["tools/readme_stats.py", "--check"],
            want_in_output="HAS NO HEADLINE LINE")

    # THE LINK FIGURE, IN ALL THREE PLACES IT APPEARS. It disagreed with
    # itself in two of them for long enough that the largest copy was 1,628
    # bytes out. Each is now generated, so each must be caught when edited.
    # DERIVED, NOT LITERAL. These used to plant "124 of 180 runs" and the
    # link moved to 178, so the replace matched nothing and both controls
    # went silently vacuous. Each now finds whatever figure is there and
    # perturbs it, so it cannot rot when the link does.
    control("a stale link figure on the front page",
            "README.md", _bump(rb"(\d+) of \d+ runs, [\d,]+ bytes"),
            ["tools/readme_stats.py", "--check"])
    control("a stale link figure in the HANDBOOK block",
            "HANDBOOK.md",
            _bump(rb"\d+ of (\d+) runs, [\d,]+ of the [\d,]+ bytes"),
            ["tools/readme_stats.py", "--check"])
    control("a stale link figure in HANDBOOK prose",
            "HANDBOOK.md", _bump(rb"links (\d+) contiguous runs"),
            ["tools/readme_stats.py", "--check"])

    # FINISHED WORK WITH NO ROW. The deliberate case declares itself with a
    # marker in its own source; removing that marker must make the source
    # look like what it would be without the declaration -- an orphan whose
    # bytes are counted nowhere. Three had accumulated before anything
    # checked, one of them a near-miss row an agent deleted.
    control("a matching source with no row and no declaration",
            "src/w4_tail_floats.cpp",
            lambda b: b.replace(b"NO MANIFEST ROW:", b"no home:", 1),
            ["tools/sweep.py", "--check"],
            want_in_output="have no row in")

    addr = first_manifest_address()
    control("an address in BOTH manifest and attempts",
            "src/attempts.txt",
            lambda b: b + nl_of(b)
            + ("src/zzz_probe.cpp %s" % addr).encode() + nl_of(b),
            ["tools/prune_attempts.py", "--check"])

    # THE ONE THAT WENT STALE FOR FOUR REVISIONS. A function that has been
    # matched must not still be listed as an open stall -- that is the stale
    # fact which changes what the next reader does, because it tells them
    # not to try.
    marker = b"**Still genuinely open, and the honest reasons:**"
    control("a MATCHED function listed as an open stall",
            "HANDBOOK.md",
            lambda b: b.replace(
                marker,
                marker + nl_of(b) + nl_of(b)
                + ("* `%s`, cannot be done." % addr).encode(), 1),
            ["tools/open_stalls.py", "--check"],
            want_in_output="ALREADY MATCH")

    # An address no file tracks cannot go stale loudly, only quietly.
    control("an open stall in neither manifest nor attempts",
            "HANDBOOK.md",
            lambda b: b.replace(
                marker,
                marker + nl_of(b) + nl_of(b) + b"* `82ABCDE0`, cannot be done.",
                1),
            ["tools/open_stalls.py", "--check"],
            want_in_output="NEITHER")

    # Deleting the heading must not silence the check that heading exists
    # for. This is the readme_stats.py hole, planted against a second tool.
    control("a DELETED open-stall heading is a failure, not a no-op",
            "HANDBOOK.md",
            lambda b: b.replace(marker, b"**Some notes:**", 1),
            ["tools/open_stalls.py", "--check"],
            want_in_output="CANNOT READ THE OPEN-STALL LIST")

    # And a list that names nothing is not an empty list, it is a broken one.
    control("an open-stall list naming NO address is a failure",
            "HANDBOOK.md",
            lambda b: _blank_open_stalls(b),
            ["tools/open_stalls.py", "--check"],
            want_in_output="NAMES NO ADDRESSES")

    same = all((ROOT / p).read_bytes() == b for p, b in before.items())
    check("every planted file is byte-identical to how it was found", same,
          "" if same else "SOMETHING WAS LEFT CORRUPTED -- see git diff")

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("A guard that cannot fail is worse than no guard: it reports")
        print("success for the same reason a working one does.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
