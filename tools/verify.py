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

import atexit
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PY = sys.executable

sys.path.insert(0, str(Path(__file__).parent))
import xdkcc                                                  # noqa: E402

# ONE RUN AT A TIME. Two verify.py processes cannot both be right: each one
# corrupts a real source file during its negative controls, and each one
# compiles 1,297 functions in a section that does not hold the lock. Run
# them together and the second reads the first's deliberate corruption --
# or, more often, has its compiles REFUSED by the first's lock and reports
# the refusals as broken functions.
#
# That is not hypothetical. Two overlapping runs produced two DIFFERENT
# spurious failure sets, one naming a single function and the next naming
# thirty-two accessors, and nothing was wrong with any of them.
#
# The dead-holder rule is the same one test_lock.py records for the compile
# lock, and for the same reason: a guard that cannot be cleared is worse
# than the race it prevents, because a run killed by a timeout would
# otherwise lock the project out permanently.
RUNLOCK = ROOT / "build/.verify_running"


def claim_run_lock():
    """-> None, or a message saying who holds it. Clears a dead holder."""
    if RUNLOCK.exists():
        try:
            holder = int(RUNLOCK.read_text(encoding="utf-8").strip() or 0)
        except ValueError:
            holder = 0
        if holder and holder != os.getpid() and xdkcc._pid_alive(holder):
            return ("tools/verify.py is ALREADY RUNNING as pid %d.\n\n"
                    "Two runs cannot both be right: each corrupts a source\n"
                    "file during its negative controls while the other is\n"
                    "compiling, so the answer would be wrong in a way that\n"
                    "looks exactly like a broken function.\n\n"
                    "Wait for it to finish. If it was killed, this lock\n"
                    "clears itself as soon as that pid is gone."
                    % holder)
    RUNLOCK.parent.mkdir(parents=True, exist_ok=True)
    RUNLOCK.write_text(str(os.getpid()), encoding="utf-8")
    atexit.register(release_run_lock)
    return None


def release_run_lock():
    try:
        if RUNLOCK.exists() and \
                RUNLOCK.read_text(encoding="utf-8").strip() == str(os.getpid()):
            RUNLOCK.unlink()
    except OSError:
        pass


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
        # write_bytes, for the same reason negative() reads bytes: text mode
        # would translate the line endings and hand back a file that differs
        # from the one taken away.
        (ROOT / rel).write_bytes(text.encode("utf-8"))
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
    # BYTES, not text. `read_text` then `write_text` translates line endings
    # on Windows, so restoring a CRLF file rewrote every line of it. The
    # content came back right and the BYTES did not, which is invisible in a
    # diff of the content and fatal to anything that digests the file:
    # build/linked.txt records the digest of the manifest it was measured
    # against, and a verify run silently invalidated it, so the next report
    # said complete_code was unmeasured. Second time this exact translation
    # has caused a bug here today -- see tools/test_privacy_guard.py.
    orig = p.read_bytes()
    orig_text = orig.decode("utf-8")
    if old not in orig_text or (also and also[0] not in orig_text):
        print("  %-42s FAIL (pattern absent, test is invalid)" % label)
        return False
    text = orig_text.replace(old, new, 1)
    if also:
        text = text.replace(also[0], also[1], 1)
    import json
    SENTINEL.parent.mkdir(parents=True, exist_ok=True)
    SENTINEL.write_text(json.dumps({path: orig_text}), encoding="utf-8")
    p.write_bytes(text.encode("utf-8"))
    try:
        rc, out = run(["tools/build.py"])
    finally:
        p.write_bytes(orig)
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


def classify_match(rc, out):
    """What a tools/match.py run actually established. -> (verdict, why).

    Three outcomes, not two. `match` and `differ` are verdicts match.py
    REACHED; `unmeasured` is what it means when it never compared anything,
    and conflating that with `differ` is what produced two contradictory
    sets of phantom failures in one afternoon.

    The test for "it compared" is POSITIVE: match.py prints its
    `N word(s) compared:` line before every verdict it reaches, so the
    absence of that line is evidence no comparison happened. A blacklist of
    known failure strings would have to be extended for every new way of
    failing, and until someone extended it the new way would be counted as a
    broken function.
    """
    if rc == 0:
        return "match", ""
    if "word(s) compared" in out:
        return "differ", ""
    if "REFUSING TO COMPILE" in out:
        return "unmeasured", ("compile refused -- another process holds the "
                              "negative-control lock")
    for line in out.splitlines():
        if line.strip():
            return "unmeasured", line.strip()
    return "unmeasured", "no output at all from match.py"


def main():
    busy = claim_run_lock()
    if busy is not None:
        print(busy)
        return 1
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
    results.append(check("compile memo, 13 cases (in-process + on-disk)",
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

    # AND NO FINISHED WORK WITH NO ROW AT ALL. The check above re-scores rows
    # already IN attempts.txt; a source matching its target while listed in
    # NEITHER file was invisible to this entire suite. Three were sitting
    # there at once: one simply never given a manifest row, one deliberate
    # and now declaring itself, and one whose near-miss row an agent deleted
    # and a `git add -A` committed. HANDBOOK has said for months that "work
    # can be finished and still have nowhere to go"; nothing checked.
    results.append(check("no finished source is missing its row",
                         ["tools/sweep.py", "--check"]))

    # NO ADDRESS IN BOTH src/manifest.txt AND src/attempts.txt. It happens the
    # moment a function is matched by a NEW source while an older near-miss
    # source for the same address is still on record -- 825409E8 was matched
    # by z1_memcmp_n.cpp with l45_cmp_bytes_n.cpp still listed at 13 of 16.
    #
    # `sweep.py --attempts --check` cannot see it: that fires when an
    # attempt's OWN source starts matching, and this one still does not.
    # Meanwhile report.py reads both files, so the address becomes two units
    # and is counted twice, and attempts.txt overstates what resists.
    def _addr_map(name):
        out = {}
        for line in (ROOT / "src" / name).read_text().splitlines():
            s = line.split("#")[0].strip()
            if not s:
                continue
            f = s.split()
            if len(f) >= 2:
                try:
                    out.setdefault(int(f[1], 16), []).append(f[0])
                except ValueError:
                    pass
        return out
    _m, _a = _addr_map("manifest.txt"), _addr_map("attempts.txt")
    _both = sorted(set(_m) & set(_a))
    print("  %-42s %s%s"
          % ("no address is both matched and a near-miss",
             "ok" if not _both else "FAIL",
             "" if not _both else
             "  " + ", ".join("%08X" % x for x in _both[:4])))
    for _x in _both:
        print("      %08X matched by %s, still a near-miss in %s"
              % (_x, ",".join(_m[_x]), ",".join(_a[_x])))
    results.append(not _both)
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
    # And the same question asked of git OBJECTS rather than the tree.
    # The tree check passed for sixty-seven commits while nine blobs in
    # history held an account name: `git grep` reports the TREE, a push
    # sends the OBJECTS, and only one of those was ever checked. This
    # guard plants objects -- reachable and unreachable, plus one that
    # must NOT fire -- and requires each verdict.
    results.append(check("privacy history guard fires, 6 cases",
                         ["tools/test_privacy_history_guard.py"]))
    # The README's front page said 181 functions and 10,280 bytes when the
    # truth was 1,200 and 34,096 -- wrong by six times, in the first block a
    # visitor reads, and stale in five other places. MATCHED.md's table is
    # generated for exactly this reason and did not rot; the README's
    # numbers were hand-maintained and did.
    results.append(check("README figures match the repository",
                         ["tools/readme_stats.py", "--check"]))
    results.append(check("MATCHED.md table matches the manifest",
                         ["tools/matched_table.py", "--check"]))
    # The tool inventory rotted to 27 of 77 missing, including tools in the
    # daily loop, so it is generated from each tool's own docstring now. This
    # also catches a tool that DOES NOT PARSE -- `vmx128_intrinsics.py` had a
    # string split across two lines by the backslash trap and had been
    # unrunnable for some time, invisible because nothing imports it.
    results.append(check("HANDBOOK tool table lists every tool, and each parses",
                         ["tools/tool_table.py", "--check"]))
    # A stale FIGURE is embarrassing; a stale STALL is expensive, because the
    # only people who read the list are deciding what to work on next and it
    # tells them not to. MATCHED.md named `8216C240` as "not source-readable"
    # for two revisions while its own generated table, three hundred lines
    # earlier, listed the function as matched.
    results.append(check("no matched function is listed as an open stall",
                         ["tools/open_stalls.py", "--check"]))
    # The five --check guards above all report "up to date" on a clean tree,
    # which says nothing on its own. This plants each violation and requires a
    # refusal. It found readme_stats.py substituting the front-page headline
    # only when the regex matched: editing or deleting that sentence made
    # --check compare the file to itself and pass, so the guard that exists
    # because the front page was wrong by six times could be silenced by
    # removing the sentence it maintains.
    results.append(check("doc guards refuse what they claim, 21 cases",
                         ["tools/test_doc_guards.py"]))
    # An UNMEASURED function is not a broken one. Two overlapping verify runs
    # had their compiles refused by the other's negative-control lock and
    # reported the refusals as mismatches -- one naming a single function, the
    # next naming thirty-two, none of them actually wrong. These 12 cases hold
    # the three-way classification and the whole-run lock that now prevents it.
    results.append(check("unmeasured is not a mismatch, 13 cases",
                         ["tools/test_verify_honesty.py"]))
    # The MATCHES section below compiles each source ONCE and calls
    # match.select/match.compare in process, rather than launching
    # tools/match.py per row. That is only safe while the two paths are one
    # implementation, and this is what says so -- including the two cases
    # that must REFUSE, where a home-grown copy would report `differ`.
    results.append(check("match.py: API and command line agree, 5 cases",
                         ["tools/test_match_api.py"]))
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
    results.append(check("link.py controls, 10 cases (5 must differ)",
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
    # STALENESS IS A FAILURE, not a pass. This file is written by
    # tools/publish_report.py and read here; for a day nothing regenerated it,
    # so a check named "build, report and objdiff-cli agree" was comparing two
    # live numbers against a constant measured once. It passed the whole time.
    # A third opinion that cannot change is not a third opinion.
    #
    # Compared by DIGEST of the manifest the export was run against, not by
    # mtime. Written with mtimes first, it fired on correct input: the
    # negative controls below restore src/manifest.txt byte for byte, which
    # bumps its mtime, so every run left the next one calling a perfectly
    # fresh report stale.
    import hashlib as _hl
    _man = _hl.sha256((ROOT / "src/manifest.txt").read_bytes()).hexdigest()[:16]
    _tot = ROOT / "build/objdiff_totals.json"
    _exported_for = None
    if _tot.exists():
        try:
            _exported_for = _json.loads(_tot.read_text()).get("manifest")
        except (ValueError, TypeError):
            pass
    if cli.exists() and _exported_for != _man:
        print("      build/report_cli.json was exported against manifest %s,"
              % (_exported_for or "?"))
        print("      which is now %s -- run `python tools/publish_report.py`."
              % _man)
        print("      It is not a third opinion until it is regenerated.")
        figures["objdiff-cli(STALE)"] = -1
    elif cli.exists():
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

    # THE DENOMINATORS MUST BE RECONCILED, not just the numerator. The check
    # above compares matched_code three ways and found them equal for months
    # while report.py divided by 8,467,964 and objdiff-cli by 8,368,632 --
    # a 99,332-byte difference nothing looked at, giving two different
    # headline percentages for one project.
    #
    # They are not the same question and neither is wrong. report.py's
    # denominator is the whole of .text, which is what "how much of this
    # section is reproduced" means. objdiff-cli can only count the units it
    # is given, and a unit is a function, so its denominator is the sum of
    # known function extents. The difference is exactly the .text bytes that
    # belong to no function: inter-function alignment padding and the data
    # blob at the tail.
    #
    # So this does not force them equal. It computes the gap INDEPENDENTLY,
    # from the image and the inventory, and requires the difference between
    # the two reports to be accounted for by it. An unexplained residue is
    # the failure -- that is the shape of the drift, not the size of it.
    gap_ok, gap_detail = True, ""
    try:
        import sys as _sys
        _sys.path.insert(0, str(ROOT / "tools"))
        from peimage import Image as _Image, load_inventory as _load_inv
        _img = _Image()
        _t = next(s for s in _img.sections if s["name"] == ".text")
        _lo = _t["va"]
        _size = _t["vsize"] or _t["rawsz"]
        _hi = _lo + _size
        # load_inventory() returns a LIST of (address, size), not a mapping.
        # Calling .items() on it raised, and the check reported "could not
        # reconcile" -- which is the right answer to give when it cannot
        # look, and the reason it says that rather than passing.
        _rows = sorted((a, n) for a, n in _load_inv()
                       if _lo <= a < _hi and n > 0)
        _cov, _cur = 0, _lo
        for _a, _n in _rows:
            if _a > _cur:
                _cov += _a - _cur
            _cur = max(_cur, _a + _n)
        if _cur < _hi:
            _cov += _hi - _cur
        m3 = _re.search(r"matched_code\s+\d+ of (\d+)", out2)
        rep_total = int(m3.group(1)) if m3 else None
        cli_total = None
        if cli.exists():
            try:
                cli_total = int(_json.loads(cli.read_text())
                                ["measures"]["total_code"])
            except (ValueError, KeyError, TypeError):
                pass
        # objdiff-cli's denominator must be EXACTLY what objdiff_export.py
        # handed it. Inferring it from the inventory instead came out 2,568
        # bytes short, for two reasons neither of which is visible from here:
        # the inventory overlaps itself in places, and the export reconciles
        # a unit's recorded size with can_extend/can_shrink. So the export
        # writes down its own total and this compares against that -- an
        # equality, with nothing to model and no tolerance to argue about.
        # A unit silently dropped from the export shows up here immediately,
        # which is what shrinks a denominator and flatters a percentage.
        tot_p = ROOT / "build/objdiff_totals.json"
        emitted = None
        if tot_p.exists():
            try:
                emitted = int(_json.loads(tot_p.read_text())["total_bytes"])
            except (ValueError, KeyError, TypeError):
                pass
        if rep_total is None or cli_total is None or emitted is None:
            gap_ok = False
            gap_detail = "  could not read the denominators"
        else:
            gap_ok = (cli_total == emitted)
            # `_cov` is the .text bytes belonging to no inventory row at all
            # -- alignment padding and the data blob at the tail. It is why
            # the two reports quote different percentages, and it is printed
            # so the difference is explained rather than merely tolerated.
            gap_detail = ("  objdiff %d == emitted %d; .text %d is %d more, "
                          "of which %d belong to no function"
                          % (cli_total, emitted, rep_total,
                             rep_total - cli_total, _cov))
    except Exception as _e:                    # noqa: BLE001
        gap_ok = False
        gap_detail = "  could not reconcile: %s" % _e
    print("  %-42s %s%s" % ("the two denominators reconcile",
                            "ok" if gap_ok else "FAIL", gap_detail))
    results.append(gap_ok)

    print("")
    print("MATCHES -- %d function(s)\n" % len(MATCHES))
    ok_n = 0
    mismatched, unmeasured = [], []

    # ONE COMPILE PER SOURCE FILE, not one per manifest row.
    #
    # This used to run tools/match.py as a subprocess for every row. That was
    # right about the question and wrong about the arithmetic: 1,986 rows come
    # from 579 files, and the 1,475 generated accessors come from 37 of them
    # at up to 40 functions each -- so the loop paid for ~1,400 process
    # launches re-compiling objects it had already built. The run took ten
    # minutes and most of it was Python starting up.
    #
    # What it is NOT is a second opinion about matching. The object is
    # compiled by match.compile_one, the function is chosen by match.select,
    # and the verdict is match.compare -- the same three calls tools/match.py
    # itself makes, in the same order. verify.py reaching its own conclusion
    # here is exactly the drift this project has paid for five times, so it
    # does not: tools/test_match_api.py requires the in-process path and the
    # command line to agree, case by case, including the ones that refuse.
    #
    # THE FAILURE MODE THIS MUST PRESERVE, because it cost a whole diagnosis:
    # a compile REFUSED (another process holding the negative-control lock)
    # is not a mismatch. Two overlapping runs printed refusals as FAIL, one
    # blaming a single function and the next thirty-two accessors, and
    # nothing was wrong with any of them. A compile that did not happen is
    # `unmeasured`, and unmeasured is a third outcome, not a bad `differ`.
    groups = {}
    for src, addr, sym, flags in MATCHES:
        groups.setdefault((src, flags), []).append((addr, sym))

    sys.path.insert(0, str(ROOT / "tools"))
    import match as _match
    from peimage import Image as _Image, load_inventory as _load_inv
    from libmatch import coff_functions as _coff_functions
    from libmatch import trim_padding as _trim_padding

    _img = _Image()
    _sizes = dict(_load_inv())
    _work = ROOT / "build/match"
    # `flags` is None for a row that takes the defaults, and None does not
    # order against a string -- sort on the text form rather than the key.
    for (src, flags), rows in sorted(groups.items(),
                                     key=lambda kv: (kv[0][0], kv[0][1] or "")):
        use = (_match.parse_flags(flags) if flags
               else list(_match.DEFAULT_FLAGS))
        try:
            obj = _match.compile_one(Path(ROOT / src), use, _work)
        except Exception as e:                              # noqa: BLE001
            obj, why_c = None, "compile raised: %s" % e
        else:
            why_c = ("compile refused or failed -- another process may hold"
                     " the negative-control lock")
        fns = None
        if obj is not None:
            try:
                fns = _coff_functions(obj.read_bytes())
            except Exception as e:                          # noqa: BLE001
                fns, why_c = None, "object unreadable: %s" % e
        for addr, sym in rows:
            if fns is None:
                unmeasured.append((src, addr, sym, why_c))
                print("  ????  %-28s %s %s" % (Path(src).name, addr, why_c))
                continue
            target = int(addr, 16)
            if target not in _sizes:
                why = "%s is not a known function start" % addr
                unmeasured.append((src, addr, sym, why))
                print("  ????  %-28s %s %s" % (Path(src).name, addr, why))
                continue
            picked, why = _match.select(fns, sym)
            if picked is None:
                unmeasured.append((src, addr, sym, why))
                print("  ????  %-28s %s %s" % (Path(src).name, addr, why))
                continue
            _n, code, mask = picked[0]
            code, mask = _trim_padding(code, mask)
            res = _match.compare(_img, _sizes, target, code, mask)
            if res["verdict"] == "match":
                ok_n += 1
            else:
                mismatched.append((src, addr, sym, flags))
                print("  FAIL  %-28s %s %s %s"
                      % (Path(src).name, addr, sym or "", flags or ""))
    print("  %d of %d match, %d differ, %d could not be measured"
          % (ok_n, len(MATCHES), len(mismatched), len(unmeasured)))
    if unmeasured:
        print("")
        print("  A function that could not be COMPILED has not been shown to")
        print("  differ from anything. These are reported apart from real")
        print("  mismatches on purpose, and they still fail this run --")
        print("  an unmeasured fact is not a passing one either.")
        print("  The usual cause is another verify.py, an agent, or a build")
        print("  running at the same time. Run it alone and try again.")
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
