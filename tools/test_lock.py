"""The negative-control lock must refuse other processes and only those.

    python tools/test_lock.py

verify.py's negative controls corrupt a real file in src/, build, and
restore. Parallel agents compile through xdkcc constantly, and anything
compiled inside that window reads text that is wrong by design -- producing
a result indistinguishable from an ordinary mismatch, which then gets
written into a manifest and believed.

Six checks, and THREE must REFUSE. A lock that never fires is decoration; a
lock that fires on its own holder, or on its children, breaks the very
builds the negative controls run -- which is what happened, and four
controls reported NOT CAUGHT because the build had failed with a refusal
message instead of the C2118 or hash mismatch each looks for.
"""

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/test_lock"
RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "probe.cpp"
    src.write_text("unsigned int f(void) { return 7u; }\n")
    held = xdkcc.LOCK.exists()
    if held:
        print("build/.negative_controls.lock already exists -- a verify.py is")
        print("running, or one was killed. Refusing to test around it.")
        return 1

    print("negative-control lock -- 7 checks, 3 must REFUSE")
    print("")

    blob, err = xdkcc.compile_obj(src, WORK / "a.obj", None, WORK)
    check("compiles normally with no lock", blob is not None, err or "")

    # Held by SOMEONE ELSE -- and that someone must be ALIVE. `getpid() + 1`
    # was used here at first, which is almost never a running process, so the
    # dead-holder rule below correctly cleared the lock and the three refusal
    # checks all passed by not refusing. A fake holder has to be real.
    other = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(600)"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    xdkcc.LOCK.parent.mkdir(parents=True, exist_ok=True)
    xdkcc.LOCK.write_text(str(other.pid), encoding="utf-8")
    other2 = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(600)"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        blob2, err2 = xdkcc.compile_obj(src, WORK / "b.obj", None, WORK)
        check("REFUSES while another pid holds the lock",
              blob2 is None and "REFUSING TO COMPILE" in (err2 or ""))
        check("the refusal names the cause, not a compiler error",
              "negative controls" in (err2 or ""))

        # Held by US: verify.py must still be able to run its own controls.
        xdkcc.LOCK.write_text(str(os.getpid()), encoding="utf-8")
        blob3, err3 = xdkcc.compile_obj(src, WORK / "c.obj", None, WORK)
        check("the HOLDER itself is not blocked", blob3 is not None,
              err3 or "")

        # THE CASE THAT ACTUALLY BROKE. Every negative control runs build.py
        # as a SUBPROCESS, which has its own pid. A pid-only check refused
        # exactly the builds the controls exist to run, so four of them
        # reported NOT CAUGHT -- the build had failed with a refusal message
        # instead of the C2118 or hash mismatch each one looks for.
        held = str(other.pid)
        xdkcc.LOCK.write_text(held, encoding="utf-8")
        os.environ["TOS_VERIFY_LOCK"] = held
        try:
            blob4, err4 = xdkcc.compile_obj(src, WORK / "d.obj", None, WORK)
            check("a CHILD of the holder is not blocked", blob4 is not None,
                  (err4 or "").splitlines()[0] if err4 else "")
        finally:
            os.environ.pop("TOS_VERIFY_LOCK", None)

        # And the inherited variable must not let anyone past a DIFFERENT
        # holder's lock, or it stops being a lock at all.
        # A different LIVE holder, with a stale inherited pid.
        xdkcc.LOCK.write_text(str(other2.pid), encoding="utf-8")
        os.environ["TOS_VERIFY_LOCK"] = held          # wrong holder
        try:
            blob5, err5 = xdkcc.compile_obj(src, WORK / "e.obj", None, WORK)
            check("a STALE inherited pid does not grant passage",
                  blob5 is None and "REFUSING TO COMPILE" in (err5 or ""))
        finally:
            os.environ.pop("TOS_VERIFY_LOCK", None)

        # A DEAD holder holds nothing. verify.py normally releases the lock,
        # but a command timeout killed one mid-run and every tool in the
        # project then refused to compile -- permanently, and with a message
        # confidently explaining that a verify was running when none was. A
        # guard that cannot be cleared is worse than the race it prevents.
        xdkcc.LOCK.write_text("999999", encoding="utf-8")
        blob6, err6 = xdkcc.compile_obj(src, WORK / "f.obj", None, WORK)
        check("a DEAD holder's lock is cleared, not obeyed",
              blob6 is not None and not xdkcc.LOCK.exists(),
              (err6 or "").splitlines()[0] if err6 else "")
    finally:
        other.kill()
        other2.kill()
        try:
            xdkcc.LOCK.unlink()
        except OSError:
            pass

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("Either agents can compile against deliberately corrupted")
        print("sources, or verify.py cannot run its own negative controls.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
