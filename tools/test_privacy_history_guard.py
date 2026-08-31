"""Prove the HISTORY checks in tools/test_privacy.py fire, and fire for the
right reasons.

    python tools/test_privacy_history_guard.py

The working-tree checks have their own guard. These are separate because
they fail differently: a working-tree rule is proved by editing a file,
while a history rule has to be proved against git OBJECTS, which is the
whole reason the history rules exist. The tree was clean and the objects
were not, for sixty-seven commits.

Each case plants a real object in this repository's database, runs the
checker, requires the expected verdict, and removes the plant. Nothing is
committed to a branch: the reachable case hangs a commit off a temporary
ref under refs/tmp/, which `rev-list --all` sees and which is deleted
again immediately.

The account names planted here are placeholders, built by concatenation so
this file contains no literal home path of its own.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TMP_REF = "refs/tmp/privacy-history-guard"

# Built, not written, so the checker does not refuse the commit that adds
# this file.
REAL_HOME = "C:" + "/Users/" + "notaplaceholder" + "/work"
SAFE_HOME = "C:" + "/Users/" + "redacted" + "/work"


def git(*args, data=None):
    return subprocess.run(["git"] + list(args), cwd=str(ROOT), input=data,
                          capture_output=True)


def run_checker():
    r = subprocess.run([sys.executable, "tools/test_privacy.py"],
                       cwd=str(ROOT), capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def plant_blob(text):
    """-> sha of a new UNREACHABLE loose object."""
    out = git("hash-object", "-w", "--stdin", data=text.encode()).stdout
    return out.decode().strip()


def plant_reachable(text):
    """-> sha of a commit on a temp ref, so its blob is REACHABLE."""
    blob = plant_blob(text)
    tree = git("mktree", data=("100644 blob %s\tplanted.txt%s"
                               % (blob, chr(10))).encode()).stdout
    tree = tree.decode().strip()
    commit = git("commit-tree", tree, "-m", "privacy guard plant").stdout
    commit = commit.decode().strip()
    git("update-ref", TMP_REF, commit)
    return commit


def cleanup():
    git("update-ref", "-d", TMP_REF)
    git("reflog", "expire", "--expire=now", "--all")
    git("gc", "--prune=now", "--quiet")


CASES = [
    # label, planted text, must-fail, line the output must contain
    ("an unreachable object holding the account name",
     "path = " + str(Path.home().name), True,
     "no unreachable git object holds it either"),
    ("an unreachable object holding a real home path",
     "path = " + REAL_HOME, True,
     "no git object contains a home-directory path"),
    ("an object holding a PLACEHOLDER home path is allowed",
     "path = " + SAFE_HOME, False,
     "no git object contains a home-directory path"),
]


def main():
    fails = 0

    rc, out = run_checker()
    if rc != 0:
        print("  FAIL the checker does not pass before planting anything:")
        print(out)
        return 1
    print("  ok   passes on the repository as it stands")

    try:
        for label, text, must_fail, needle in CASES:
            plant_blob(text)
            rc, out = run_checker()
            failed = rc != 0
            named = any(line.strip().startswith("FAIL") and needle in line
                        for line in out.splitlines())
            ok = (failed == must_fail) and (named == must_fail)
            fails += 0 if ok else 1
            print("  %-4s %s" % ("ok" if ok else "FAIL", label))
            if not ok:
                print(out)
            cleanup()

        # And the reachable case, which is the one a push would publish.
        plant_reachable("path = " + str(Path.home().name))
        rc, out = run_checker()
        named = any(line.strip().startswith("FAIL")
                    and "REACHABLE git object" in line
                    for line in out.splitlines())
        ok = rc != 0 and named
        fails += 0 if ok else 1
        print("  %-4s a REACHABLE object holding the account name" %
              ("ok" if ok else "FAIL"))
        if not ok:
            print(out)
    finally:
        cleanup()

    rc, out = run_checker()
    if rc != 0:
        print("  FAIL the plants were not cleaned up:")
        print(out)
        fails += 1
    else:
        print("  ok   every plant was removed and the repository passes again")

    print("")
    print("  %d failure(s) of %d check(s)" % (fails, len(CASES) + 3))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
