"""The negative-control lock must refuse other processes and only those.

    python tools/test_lock.py

verify.py's negative controls corrupt a real file in src/, build, and
restore. Parallel agents compile through xdkcc constantly, and anything
compiled inside that window reads text that is wrong by design -- producing
a result indistinguishable from an ordinary mismatch, which then gets
written into a manifest and believed.

Four checks, and TWO of them must REFUSE. A lock that never fires is
decoration; a lock that fires on its own holder deadlocks verify.py itself,
which is the failure that would be found last.
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

    print("negative-control lock -- 4 checks, 2 must REFUSE")
    print("")

    blob, err = xdkcc.compile_obj(src, WORK / "a.obj", None, WORK)
    check("compiles normally with no lock", blob is not None, err or "")

    # Held by SOMEONE ELSE: must refuse.
    xdkcc.LOCK.parent.mkdir(parents=True, exist_ok=True)
    xdkcc.LOCK.write_text(str(os.getpid() + 1), encoding="utf-8")
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
    finally:
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
