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
    # BYTES, not text. Read-as-text-and-write-back translates line endings on
    # Windows, so restoring a CRLF file rewrote every line of it: the content
    # was right, `git status` said modified, and the noise sat on top of a
    # real problem. A restore has to give back exactly what it took.
    orig = VICTIM.read_bytes()
    try:
        VICTIM.write_bytes(orig + ("\n" + text + "\n").encode("utf-8"))
        return run_privacy()
    finally:
        VICTIM.write_bytes(orig)
        # Belt and braces, FROM HEAD -- not from the index.
        #
        # This said `git checkout -- MATCHED.md`, which restores from the
        # INDEX, and that turned the cleanup into the thing that made a plant
        # survive. Anything running `git add` while this test is mid-plant --
        # `git add -A` in another window, an editor, a hook -- stages the
        # planted line, and the restore then faithfully wrote the plant back
        # out. It happened: a planted account name sat in MATCHED.md through
        # three green verify runs afterwards, because every later run read the
        # polluted file as its own `orig` and restored that.
        #
        # A restore that can reinstate what it is restoring from is worse than
        # no restore, because the tree then looks clean to the process that
        # dirtied it. HEAD cannot hold a plant: it is never committed, and the
        # pre-commit hook refuses if it ever were.
        if VICTIM.read_bytes() != orig:
            subprocess.run(["git", "checkout", "HEAD", "--", str(VICTIM)],
                           cwd=str(ROOT), capture_output=True)


def main():
    print("privacy guard -- 4 plants, every one must FAIL the check")
    print("")

    # What the victim file looked like before any of this, to the byte.
    # Checked again at the end, because a test that plants an identifying
    # string into a tracked file has to prove it took it back out -- and once
    # it did not.
    start = VICTIM.read_bytes()

    check("clean tree passes", run_privacy() == 0)

    # The account-name plant can only fire where the account-name check runs.
    # On a hosted runner that check is `n/a` -- the home account is a service
    # account, not a person -- so planting it would prove nothing and the
    # guard would report a failure that is really an environment.
    local_only = test_privacy.local_gate_reason()
    account = Path.home().name
    if local_only:
        print("  n/a  planting this machine's account name FAILS  -- %s"
              % local_only)
        print("       the check it guards does not run here either; both are")
        print("       enforced on a developer machine and in the pre-commit")
        print("       hook, and this run does not clear them")
    else:
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

    restored = VICTIM.read_bytes() == start
    check("%s is byte-identical to how it was found" % VICTIM.name, restored,
          "" if restored else "A PLANT MAY STILL BE IN IT -- see git diff")

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("The privacy check cannot see something it claims to catch,")
        print("or this test did not clean up after itself.")
        print("Do not rely on it before publishing.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
