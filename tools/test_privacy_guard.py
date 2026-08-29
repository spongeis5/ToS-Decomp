"""The privacy check must FAIL on each thing it claims to catch.

    python tools/test_privacy_guard.py

tools/test_privacy.py passing tells you nothing on its own -- a check that
cannot fail reports success for the same reason a working one does. This
plants each violation in turn, requires a failure, and removes it.

Four plants, four required failures. The account-name and home-path plants
go into a real tracked file (restored in a `finally`, and the file is
restored from git as a second line of defence). The identity plants are
checked against the same matcher test_privacy uses, without touching git
history, because rewriting history to test a checker would be a far worse
idea than the thing being tested.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import test_privacy

RESULTS = []
# A tracked file that is pure prose, so a planted line cannot break a build.
VICTIM = ROOT / "MATCHED.md"


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def run_privacy():
    r = subprocess.run([sys.executable, "tools/test_privacy.py"],
                       cwd=str(ROOT), capture_output=True, text=True)
    return r.returncode


def plant(text):
    orig = VICTIM.read_text(encoding="utf-8")
    try:
        VICTIM.write_text(orig + "\n" + text + "\n", encoding="utf-8")
        return run_privacy()
    finally:
        VICTIM.write_text(orig, encoding="utf-8")
        # Belt and braces: if the write above failed somehow, git has it.
        subprocess.run(["git", "checkout", "--", str(VICTIM)],
                       cwd=str(ROOT), capture_output=True)


def main():
    print("privacy guard -- 4 plants, every one must FAIL the check")
    print("")

    check("clean tree passes", run_privacy() == 0)

    account = Path.home().name
    rc = plant("An accidental mention of %s in a doc." % account)
    check("planting this machine's account name FAILS", rc != 0,
          "account name redacted from this output")

    rc = plant("See C:/Users/someone/Downloads/thing for details.")
    check("planting a Windows home path FAILS", rc != 0)

    rc = plant("Run /home/someone/scripts/build.sh first.")
    check("planting a POSIX home path FAILS", rc != 0)

    # The identity rule, tested against the matcher rather than by writing
    # commits. Rewriting history to test a history checker is a worse idea
    # than the problem it guards.
    m = test_privacy.ALLOWED_EMAIL
    real = ["someone@gmail.com", "first.last@company.co.uk",
            "dev@example.org"]
    ok_addrs = ["1234+user@users.noreply.github.com",
                "user@users.noreply.github.com",
                "noreply@anthropic.com"]
    check("a real mailbox is REJECTED as a commit identity",
          all(not m.match(a) for a in real),
          "%d address shape(s) tried" % len(real))
    check("privacy addresses are accepted",
          all(m.match(a) for a in ok_addrs))

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("The privacy check cannot see something it claims to catch.")
        print("Do not rely on it before publishing.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
