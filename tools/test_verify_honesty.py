"""verify.py must not report an UNMEASURED function as a broken one.

    python tools/test_verify_honesty.py

Two verify.py runs were started a few minutes apart while agents were
compiling. The first reported one broken function; the second reported
thirty-two. Both were clean -- every one of those functions matches on its
own, and the two runs did not even name the same ones.

What happened is that `xdkcc` REFUSES to compile while verify.py's negative
controls hold their lock, because those controls deliberately corrupt a file
in `src/`. The refusal is correct and its message says precisely what it
means:

    "the result would look like an ordinary mismatch rather than a race"

and verify.py's MATCHES loop then threw that message away and printed FAIL.
The prediction and the misreading were in the same repository, four hundred
lines apart.

So there are two fixes and this file is the control for both:

  * `classify_match` returns THREE outcomes. A verdict match.py reached is
    `match` or `differ`; anything where it never compared is `unmeasured`,
    reported apart and never counted as a difference. The test for "it
    compared" is positive -- match.py's own `N word(s) compared:` line --
    so a failure mode nobody has seen yet lands in `unmeasured` instead of
    being scored as a broken function.
  * a whole-run lock, so a second verify.py refuses to start rather than
    producing a wrong answer. Its dead-holder rule is tested too: a guard
    that cannot be cleared is worse than the race it prevents, which
    `test_lock.py` records having learned the hard way about the other lock.

The real refusal text is taken from `xdkcc` itself rather than pasted here,
so that rewording the message cannot silently disarm the classifier.
"""

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import verify
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def real_refusal_text():
    """The refusal xdkcc actually emits, obtained by provoking one.

    Not a copy of the string. If someone rewords the message, this test
    keeps testing the real thing -- and if the refusal stops happening at
    all, the check below fails rather than passing on a stale constant.
    """
    work = ROOT / "build/test_verify_honesty"
    work.mkdir(parents=True, exist_ok=True)
    src = work / "probe.cpp"
    src.write_text("unsigned int f(void) { return 7u; }\n", encoding="utf-8")
    if xdkcc.LOCK.exists():
        return None, "the compile lock is already held; refusing to test"
    other = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(600)"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        xdkcc.LOCK.parent.mkdir(parents=True, exist_ok=True)
        xdkcc.LOCK.write_text(str(other.pid), encoding="utf-8")
        blob, err = xdkcc.compile_obj(src, work / "a.obj", None, work)
        if blob is not None:
            return None, "xdkcc did NOT refuse while another pid held the lock"
        return err, None
    finally:
        other.kill()
        try:
            xdkcc.LOCK.unlink()
        except OSError:
            pass


# A real match.py mismatch, trimmed. The `word(s) compared` line is the part
# that matters: it is the evidence a comparison actually happened.
DIFFERED = """
target  82151690  44 byte(s)
ours    ?LargerOf@@YAHPBUOwner40@@@Z             44 byte(s)

 X 82151690  want 7c0802a6  mflr r0
            got  38600000  li r3,0

11 word(s) compared: 10 identical, 1 differ, 0 differ in a relocated word (expected)

NO MATCH.
"""

ALL_RELOCATED = """
target  826C0FB8  4 byte(s)
ours    ?Nth@@YAPAXPAX@Z                         4 byte(s)

1 word(s) compared: 0 identical, 0 differ, 1 differ in a relocated word (expected)

NOT A MATCH -- all 1 word(s) are relocated, so nothing was
actually verified.
"""

TRACEBACK = """Traceback (most recent call last):
  File "tools/match.py", line 282, in main
    src = Path(argv[1])
IndexError: list index out of range
"""


def main():
    print("verify.py must separate 'differs' from 'could not be measured'")
    print("")

    v, why = verify.classify_match(0, "\n11 word(s) compared: 11 identical")
    check("an exact match is 'match'", v == "match", why)

    v, _ = verify.classify_match(1, DIFFERED)
    check("a real byte mismatch is 'differ'", v == "differ",
          "got %r" % v)

    # The all-relocated case DID compare, and match.py refuses it on the
    # merits. That is a verdict, not a failure to measure.
    v, _ = verify.classify_match(1, ALL_RELOCATED)
    check("an all-relocated refusal is a VERDICT, not unmeasured",
          v == "differ", "got %r" % v)

    refusal, err = real_refusal_text()
    if refusal is None:
        check("a compile refusal is 'unmeasured'", False, err)
    else:
        v, why = verify.classify_match(1, refusal)
        check("a compile refusal is 'unmeasured', with the reason",
              v == "unmeasured" and "negative-control lock" in why,
              "got %r / %r" % (v, why))

    v, why = verify.classify_match(1, TRACEBACK)
    check("a crash in match.py is 'unmeasured', not a broken function",
          v == "unmeasured" and "Traceback" in why, "got %r" % v)

    v, why = verify.classify_match(1, "")
    check("silence is 'unmeasured' and says so",
          v == "unmeasured" and "no output" in why, "got %r / %r" % (v, why))

    # THE POINT OF THE POSITIVE TEST. A failure mode nobody has written down
    # must land in `unmeasured`, not be scored as a broken function.
    v, _ = verify.classify_match(1, "cl : Command line error D8021 : bad flag")
    check("an UNKNOWN failure is 'unmeasured', not 'differ'",
          v == "unmeasured", "got %r" % v)

    print("")
    print("the whole-run lock -- a second verify.py must refuse to start")
    print("")

    # THE LOCK LOGIC IS TESTED ON A LOCK OF ITS OWN. This file runs as one of
    # verify.py's own checks, so the real lock is held by the parent -- by the
    # very mechanism under test. Testing the real path would either fail on a
    # correct tree or, worse, release the parent's lock and reopen the race
    # this exists to close. The path is a variable; the logic is the subject.
    real = verify.RUNLOCK
    real_before = real.read_bytes() if real.exists() else None
    work = ROOT / "build/test_verify_honesty"
    work.mkdir(parents=True, exist_ok=True)
    verify.RUNLOCK = work / ".verify_running"
    try:
        if verify.RUNLOCK.exists():
            verify.RUNLOCK.unlink()
        other = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(600)"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            verify.RUNLOCK.write_text(str(other.pid), encoding="utf-8")
            busy = verify.claim_run_lock()
            check("REFUSES while a live verify.py holds the lock",
                  busy is not None and "ALREADY RUNNING" in (busy or ""))
            check("the refusal names the holder's pid",
                  busy is not None and str(other.pid) in busy)

            # A DEAD holder holds nothing, or a killed run locks the project
            # out permanently. Same rule as the compile lock.
            verify.RUNLOCK.write_text("999999", encoding="utf-8")
            busy2 = verify.claim_run_lock()
            check("a DEAD holder's lock is cleared, not obeyed",
                  busy2 is None and verify.RUNLOCK.exists()
                  and verify.RUNLOCK.read_text().strip() == str(os.getpid()))

            # And the holder must be able to re-enter its own lock.
            busy3 = verify.claim_run_lock()
            check("the holder itself is not blocked", busy3 is None)
        finally:
            other.kill()
            verify.release_run_lock()
        check("the run lock is released afterwards",
              not verify.RUNLOCK.exists())
    finally:
        verify.RUNLOCK = real

    now = real.read_bytes() if real.exists() else None
    check("the REAL run lock was left exactly as it was found",
          now == real_before,
          "" if now == real_before else "this test disturbed a live verify.py")

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("A function that could not be COMPILED has not been shown to")
        print("differ from anything. Reporting one as the other is how two")
        print("clean runs came to name thirty-three phantom failures.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
