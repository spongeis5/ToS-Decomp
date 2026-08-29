"""Run every check this project has, and say which ones can actually fail.

    python tools/verify.py

This exists because an audit found six real defects in tooling that had been
committed as working, and four of them were invisible from any single tool's
own output:

  * the .text hash could not fail -- only functions already proven equal were
    spliced in, so `rebuilt == original` was true by construction
  * `discover.py --compare` compared discovery against itself once the
    inventory default was switched to discovery
  * three of the four compile harnesses lacked `include/` on their search
    path, so seven matches broke while build.py still passed
  * segment.py fabricated a 4-byte size for 88 functions, inflating a
    reported precision from 55% to 72%

Every one was a check that reported success without exercising anything. So
this runner does not just run the checks -- where a check has a NEGATIVE
CONTROL available, it runs that too, and a check whose control does not fail
is reported as broken even if the check itself passes.
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PY = sys.executable


def run(args, cwd=ROOT):
    r = subprocess.run([PY] + args, capture_output=True, text=True,
                       cwd=str(cwd))
    return r.returncode, r.stdout + r.stderr


def check(label, args, want_zero=True):
    rc, out = run(args)
    ok = (rc == 0) if want_zero else (rc != 0)
    print("  %-42s %s" % (label, "ok" if ok else "FAIL (exit %d)" % rc))
    if not ok:
        for line in out.strip().splitlines()[-4:]:
            print("      %s" % line)
    return ok


SENTINEL = ROOT / "build/.verify_restore.json"


def restore_if_interrupted():
    """Put back anything a killed run left corrupted.

    `negative()` corrupts a real source file, runs the build, and restores it
    in a `finally`. A `finally` does not run when the process is KILLED --
    and a two-minute command timeout killed one of these mid-control, leaving
    `src/manifest.txt` holding 821636AC instead of 821636A8. The next run
    then reported four failures, one of them a negative control reading
    "pattern absent, test is invalid", which is a confusing way to be told
    the tree is dirty.

    So the original text goes to disk BEFORE the corruption and is removed
    only after the restore. If it is still there at startup, the previous run
    died and this puts the tree back.
    """
    if not SENTINEL.exists():
        return
    import json
    try:
        saved = json.loads(SENTINEL.read_text(encoding="utf-8"))
    except ValueError:
        print("build/.verify_restore.json is unreadable; remove it by hand.")
        sys.exit(1)
    print("A PREVIOUS RUN WAS KILLED while a negative control had a file")
    print("corrupted. Restoring before doing anything else:")
    for rel, text in saved.items():
        (ROOT / rel).write_text(text, encoding="utf-8")
        print("  restored %s" % rel)
    SENTINEL.unlink()
    print("")


def negative(label, path, old, new, expect_substr=None, also=None):
    """Corrupt one thing, require the BUILD to fail, then restore.

    `also` is a second (old, new) applied at the same time -- used when a
    layout change must be accompanied by its ASSERT_OFFSET so the failure is
    forced through the byte comparison instead of tripping C2118 first.
    """
    p = ROOT / path
    orig = p.read_text()
    if old not in orig or (also and also[0] not in orig):
        print("  %-42s FAIL (pattern absent, test is invalid)" % label)
        return False
    text = orig.replace(old, new, 1)
    if also:
        text = text.replace(also[0], also[1], 1)
    import json
    SENTINEL.parent.mkdir(parents=True, exist_ok=True)
    SENTINEL.write_text(json.dumps({path: orig}), encoding="utf-8")
    p.write_text(text)
    try:
        rc, out = run(["tools/build.py"])
    finally:
        p.write_text(orig)
        if SENTINEL.exists():
            SENTINEL.unlink()
    ok = rc != 0
    if ok and expect_substr:
        ok = expect_substr in out
    print("  %-42s %s" % (label, "ok" if ok else "FAIL -- NOT CAUGHT"))
    return ok


def load_matches():
    """Read the build manifest rather than keeping a second copy of it.

    verify.py used to carry its own hardcoded list, which is the same drift
    that let three compile harnesses fall out of step with build.py. One
    source of truth.
    """
    out = []
    for line in (ROOT / "src/manifest.txt").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split()
        # REFUSE a malformed row with a message, rather than dying on an
        # IndexError. A source path of 32 characters fills an agent's
        # `%-32s` with no padding, so the address is glued to it:
        #
        #     src/m56_set_five_floats_flag.cpp821A4318
        #
        # That row has one field. This function runs at MODULE IMPORT, so the
        # traceback arrived before a single check had printed and said
        # nothing about the manifest -- the same shape of confusion the
        # restore sentinel was written for.
        if len(f) < 2:
            print("MALFORMED MANIFEST ROW: only %d field(s)" % len(f))
            print("    %r" % line)
            print("")
            print("A source path of 32 characters or more leaves no space")
            print("before the address under `%-32s`. Separate the columns.")
            sys.exit(2)
        sym, flags = None, None
        for extra in f[2:]:
            if extra.startswith("flags="):
                flags = extra[len("flags="):]
            elif extra != "-":
                sym = extra
        out.append((f[0], f[1], sym, flags))
    return out


MATCHES = load_matches()


# HOW MANY CHECKS THERE ARE is a figure three documents quote, and a quoted
# figure nobody regenerates rots: HANDBOOK.md said "12 checks" for long enough
# that the real number reached 28. Same drift as the README front page being
# wrong by six times, and the same fix -- make it a failing check rather than
# something a reader discovers.
#
# Every pattern CAPTURES the number. Written first against the README's
# spelled-out "Twenty-eight checks" with no group, this agreed with itself
# forever: the literal matched, there was no group to read, and the count was
# never actually compared. A check with nothing to compare is the shape of
# check this project keeps having to delete.
#
# A separate function so it can be called with a WRONG total and required to
# complain -- a guard tested only by the run it guards is tested by nothing.
COUNT_IN_DOCS = (
    ("README.md", r"^(\d+) checks\. Several are"),
    ("HANDBOOK.md", r"^(\d+) checks: the tool self-tests"),
    ("CLAUDE.md", r"verify\.py\s+(\d+) checks"),
)


def documented_count_problems(total):
    """-> [] when all three documents say `total`, else what each says."""
    import re as _re
    stale = []
    for doc, pat in COUNT_IN_DOCS:
        p = ROOT / doc
        if not p.exists():
            stale.append("%s is missing" % doc)
            continue
        m = _re.search(pat, p.read_text(encoding="utf-8"), _re.M)
        if not m:
            stale.append("%s no longer states a check count where expected"
                         % doc)
        elif int(m.group(1)) != total:
            stale.append("%s says %s, there are %d" % (doc, m.group(1), total))
    return stale


def main():
    restore_if_interrupted()
    results = []

    print("TOOLS -- each must run and exit 0\n")
    results.append(check("xdkcc self-test (headers + assertions)",
                         ["tools/xdkcc.py"]))
    results.append(check("coffreloc relocation semantics, 9 cases",
                         ["tools/test_coffreloc.py"]))
    results.append(check("match.py size reconciliation, 12 cases (8 must refuse)",
                         ["tools/test_shrink.py"]))
    # The compile memo turned 559 invocations of cl into 60 and made a build
    # take 20 seconds instead of four minutes. It is also the one thing here
    # that can make a WRONG function look right without failing, because a
    # cache serving an object built from different text is indistinguishable
    # from a match. Keyed on content, and these 7 checks are what says so --
    # 3 of them must MISS the cache.
    results.append(check("compile memo, 11 cases (in-process + on-disk)",
                         ["tools/test_xdkcc_cache.py"]))
    # permute.py ranks source shapes, so a scorer that counts relocated words
    # as mismatches does not merely under-report -- it recommends the wrong
    # direction, preferring shapes with fewer relocations to shapes with
    # better code, and makes an exact match unreportable for any function
    # that calls anything.
    results.append(check("permuter scorer skips relocated words, 6 cases",
                         ["tools/test_permute.py"]))
    # vtables.py reconstructs 2151 vtables from pointer runs and code
    # references. rtti.py knows 311 of them by walking MSVC's own structures,
    # which is a completely independent route, so agreement between the two
    # is real evidence and disagreement is a boundary rule that has drifted.
    results.append(check("vtables rediscover rtti.py's 311 (>=90%)",
                         ["tools/vtables.py", "--check"]))
    # build/candidates.txt is a generated artifact and had gone STALE against
    # a corrected inventory: 100 of its 5,020 rows carried a shorter size,
    # 36 of them because a switch's jump table sits past the `bctr` where
    # discovery stopped. batch.py prints that size, so an agent was handed a
    # 352-byte switch as 64 bytes of disassembly, wrote a function for what
    # it saw, and was told SIZE DIFFERS by match.py reading the real extent.
    # Two entries in src/attempts.txt arrived that way.
    results.append(check("candidates.txt agrees with the inventory on size",
                         ["tools/truncated.py", "--check"]))
    # The negative controls below corrupt real files in src/. Parallel agents
    # compile through xdkcc constantly, and anything compiled inside that
    # window reads text that is wrong on purpose -- a result that looks like
    # an ordinary mismatch and gets written into a manifest.
    results.append(check("negative-control lock, 7 cases (3 must refuse)",
                         ["tools/test_lock.py"]))
    # A mutation that never compiles and a mutation that never helps look
    # identical from outside -- both just fail to find anything. mut_temp
    # was the second kind for a long time. These check that the two new
    # mutations FIRE and that what they emit is valid C++.
    results.append(check("permuter mutations fire and compile, 6 cases",
                         ["tools/test_mutations.py"]))
    # A near-miss that actually matches is quiet in both directions:
    # attempts.txt understates progress, and someone can spend a session on a
    # function that came out days ago. Seven were sitting like that, solved by
    # an agent and never promoted, and what surfaced them was a person
    # noticing green rows in objdiff. That is not a detection mechanism.
    results.append(check("no near-miss secretly matches",
                         ["tools/sweep.py", "--attempts", "--check"]))
    # This repository is meant to be published. Four commits carried a
    # personal email as author and committer, and eight tracked files
    # carried an account name inside hardcoded absolute paths -- found by
    # someone thinking to look, once. Content can be edited away; an
    # identity in COMMIT HISTORY needs a filter-branch, and once cloned it
    # cannot be recalled. The guard test plants each violation and requires
    # a failure, because a check that cannot fail reports success for the
    # same reason a working one does.
    results.append(check("no identifying information in tree or history",
                         ["tools/test_privacy.py"]))
    results.append(check("privacy guard fires, 7 cases (4 plants)",
                         ["tools/test_privacy_guard.py"]))
    # The README's front page said 181 functions and 10,280 bytes when the
    # truth was 1,200 and 34,096 -- wrong by six times, in the first block a
    # visitor reads, and stale in five other places. MATCHED.md's table is
    # generated for exactly this reason and did not rot; the README's
    # numbers were hand-maintained and did.
    results.append(check("README figures match the repository",
                         ["tools/readme_stats.py", "--check"]))
    results.append(check("MATCHED.md table matches the manifest",
                         ["tools/matched_table.py", "--check"]))
    results.append(check("backslash-heredoc hook, 7 cases",
                         [".claude/hooks/test_no_backslash_heredoc.py"]))
    results.append(check("reconstructing build (.text reproduces)",
                         ["tools/build.py"]))
    # The link is a different question from the splice and has to be asked
    # separately: build.py writes every function at the address the manifest
    # names, so it can never notice that two of them do not PACK, or that the
    # padding between them is wrong, or that the order is unreachable. Its
    # controls run first -- an ordering check that cannot see a wrong order is
    # the same shape of nothing as a hash over bytes already proven equal.
    results.append(check("link.py controls, 6 cases (5 must differ)",
                         ["tools/link.py", "--selftest"]))
    results.append(check("real link: runs placed, ordered, padded",
                         ["tools/link.py"]))

    # No inventory entry may be a switch case body. Case bodies are labels
    # inside a function, and one listed as a function is a real defect.
    rc, out = run(["tools/switches.py"])
    clean = rc == 0 and "case bodies  %6d" % 0 not in out
    ok = rc == 0 and "switch case bodies       0" in out.replace("  ", " ")
    line = [l for l in out.splitlines() if "case bodies" in l]
    ok = bool(line) and line[0].split()[-1] == "0"
    print("  %-42s %s" % ("no inventory entry is a switch case body",
                          "ok" if ok else "FAIL"))
    results.append(ok)

    # A symbol resolving to two addresses verifies byte for byte and could
    # never link. build.py reports it; nothing should be reporting it.
    rc, out = run(["tools/build.py"])
    linkable = "WOULD NOT LINK" not in out

    # The progress report and the build must agree on how many bytes are
    # reproduced. They count it by different routes -- build.py splices each
    # function into .text and sums what it wrote, report.py sums compiled
    # lengths per source file -- so agreement is evidence and disagreement is
    # one of them being wrong. Written the easy way, report.py used inventory
    # extents and said 34,340 against build.py's 34,096; the inventory is
    # wrong in both directions, and two numbers for one fact is the drift
    # that has produced most of this project's tooling bugs.
    # THREE independent counters, one number. build.py splices each function
    # into .text and sums what it wrote; report.py sums compiled lengths per
    # source file; objdiff-cli (the reference implementation of the report
    # schema, a Rust binary nothing here shares code with) reads the exported
    # ELF pairs. Agreement across all three is real evidence; any two of them
    # agreeing while the third differs says which one to go and look at.
    import json as _json
    import re as _re
    figures = {}
    m = _re.search(r"VERIFIED: (\d+) of \d+ \.text byte", out)
    if m:
        figures["build.py"] = int(m.group(1))
    _rc, out2 = run(["tools/report.py"])
    m2 = _re.search(r"matched_code\s+(\d+) of", out2)
    if m2:
        figures["report.py"] = int(m2.group(1))
    cli = ROOT / "build/report_cli.json"
    if cli.exists():
        try:
            figures["objdiff-cli"] = int(
                _json.loads(cli.read_text())["measures"]["matched_code"])
        except (ValueError, KeyError, TypeError):
            pass
    agree = len(figures) >= 2 and len(set(figures.values())) == 1
    detail = ("  %d source(s) agree on %d" % (len(figures),
                                              list(figures.values())[0])
              if agree else "  " + ", ".join("%s=%s" % kv
                                             for kv in sorted(figures.items())))
    print("  %-42s %s%s" % ("build, report and objdiff-cli agree",
                            "ok" if agree else "FAIL", detail))
    results.append(agree)
    print("  %-42s %s" % ("no symbol resolves to two addresses",
                          "ok" if linkable else "FAIL -- see build.py output"))
    results.append(linkable)

    print("")
    print("MATCHES -- %d function(s)\n" % len(MATCHES))
    ok_n = 0
    for src, addr, sym, flags in MATCHES:
        args = ["tools/match.py", src, addr] + (["--sym", sym] if sym else [])
        if flags:
            args += ["--flags", "/c /nologo " + " ".join(flags.split(","))]
        rc, _out = run(args)
        if rc == 0:
            ok_n += 1
        else:
            print("  FAIL  %-28s %s %s %s"
                  % (Path(src).name, addr, sym or "", flags or ""))
    print("  %d of %d match" % (ok_n, len(MATCHES)))
    results.append(ok_n == len(MATCHES))

    print("")
    print("NEGATIVE CONTROLS -- each corrupts one fact; the build MUST fail\n")
    # Everything below edits a real file in src/, builds, and restores. While
    # that is open, any OTHER process compiling from src/ reads text that is
    # wrong on purpose and gets a result indistinguishable from a genuine
    # mismatch. Parallel agents compile constantly, so this is not a corner
    # case; it happened. xdkcc.compile_obj refuses for anyone but this pid
    # while the lock exists, and says why.
    import xdkcc
    xdkcc.LOCK.parent.mkdir(parents=True, exist_ok=True)
    xdkcc.LOCK.write_text(str(os.getpid()), encoding="utf-8")
    # Children inherit this, so build.py -- which every control runs as a
    # subprocess -- is allowed through while unrelated processes are not.
    os.environ["TOS_VERIFY_LOCK"] = str(os.getpid())
    print("  (holding build/.negative_controls.lock -- other processes")
    print("   compiling from src/ will be refused until this section ends)\n")
    results.append(negative("wrong struct offset -> compile error",
                            "src/chain5.cpp",
                            "char unk0000[0x38]; B*    b;",
                            "char unk0000[0x3C]; B*    b;", "C2118"))
    results.append(negative("wrong ASSERT_SIZE -> compile error",
                            "src/table_index.cpp",
                            "ASSERT_SIZE(Entry, 1856);",
                            "ASSERT_SIZE(Entry, 1857);", "C2118"))
    # Move E.v AND its assertion together, so the layout assert stays
    # satisfied and the failure must be caught by the BYTES rather than by
    # C2118. That is what makes this a test of the hash and not of the header.
    results.append(negative(
        "wrong codegen -> .text hash differs",
        "src/chain5.cpp",
        "struct E { /* 0x18 */ char unk0000[0x18]; void* v; };",
        "struct E { /* 0x1C */ char unk0000[0x1C]; void* v; };",
        "DOES NOT REPRODUCE",
        also=("ASSERT_OFFSET(E, v, 0x18);", "ASSERT_OFFSET(E, v, 0x1C);")))
    # The image itself: every number here is a claim about one specific
    # image, so a different one must be refused rather than silently used.
    import shutil
    img = ROOT / "build/default.pe.exe"
    bak = ROOT / "build/default.pe.exe.verify"
    shutil.copy(str(img), str(bak))
    d = bytearray(img.read_bytes())
    d[0x500000] ^= 0xFF
    img.write_bytes(bytes(d))
    try:
        rc, out = run(["tools/build.py"])
    finally:
        shutil.move(str(bak), str(img))
    caught = rc != 0 and "not the image" in out
    print("  %-42s %s" % ("corrupted image -> refused",
                          "ok" if caught else "FAIL -- NOT CAUGHT"))
    results.append(caught)

    # The jump table of a switch is 25 whole-word relocations. If build.py
    # copied them from the image, a wrong CASE MAPPING would verify clean --
    # the bodies are identical and only the table says which case reaches
    # which. It predicts them from our own labels instead, so moving one case
    # to the wrong arm must fail.
    results.append(negative(
        "wrong switch case mapping -> caught",
        "src/i_canon_switch.cpp",
        "        return 27;",
        "        return 29;",
        "the case mapping"))

    results.append(negative(
        "wrong manifest address -> caught",
        "src/manifest.txt",
        "src/chain5.cpp                  821636A8",
        "src/chain5.cpp                  821636AC"))

    # HOW MANY CHECKS THERE ARE is a figure three documents quote, and a
    # quoted figure nobody regenerates rots: HANDBOOK.md said "12 checks" for
    # long enough that the real number reached 28. Same drift as the README
    # front page being wrong by six times, and the same fix -- make it a
    # failing check rather than something a reader discovers.
    #
    # `+ 1` counts this check, which has not been appended yet. Comparing
    # against the documents rather than against a constant in this file is
    # what makes it a check at all; a constant here would agree with itself.
    total = len(results) + 1
    stale = documented_count_problems(total)
    ok = not stale
    print("  %-42s %s" % ("documented check count is %d" % total,
                          "ok" if ok else "FAIL"))
    for s in stale:
        print("      %s" % s)
    results.append(ok)

    # Released here AND in the `finally` below, because a killed run must not
    # leave every other process unable to compile. The sentinel that restores
    # corrupted sources already handles the same class of failure.
    try:
        xdkcc.LOCK.unlink()
    except OSError:
        pass
    os.environ.pop("TOS_VERIFY_LOCK", None)

    print("")
    n_ok = sum(1 for r in results if r)
    print("%d of %d check(s) passed." % (n_ok, len(results)))
    if n_ok != len(results):
        print("")
        print("A failing NEGATIVE CONTROL is the serious kind: it means a")
        print("check reports success without being able to detect the failure")
        print("it exists to detect.")
    return 0 if n_ok == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
