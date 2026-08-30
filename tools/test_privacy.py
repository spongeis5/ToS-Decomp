"""Refuse to let identifying information reach a public repository.

    python tools/test_privacy.py

This project is meant to be published. Four commits carried a personal email
address as author and committer, and eight tracked files carried the
author's Windows account name inside hardcoded absolute paths. Both were
found by looking, once, because someone thought to ask -- which is not a
mechanism. This is the mechanism.

NOTHING PERSONAL IS WRITTEN IN THIS FILE, and that is a requirement rather
than a nicety: a checker that blocklists a name and an address would publish
both to every reader of the repository it is meant to protect. So every rule
is either DERIVED at runtime or an ALLOWLIST of shapes known to be safe.

  * The account name comes from `Path.home().name` on the machine running
    the check. It is never stored.
  * Commit identities are checked against a PATTERN -- GitHub's
    `<id>+<user>@users.noreply.github.com` privacy form, or an explicit
    noreply -- rather than against a list of forbidden addresses.
  * Home-directory paths are matched by shape: `C:/Users/<anything>/`,
    `/home/<anything>/`, `/Users/<anything>/`.

Run by tools/verify.py and by the pre-commit hook, so the information cannot
enter history in the first place. History already written is a separate
problem and a much worse one: it needs a filter-branch and a force-push, and
on a repository anyone has cloned it cannot be recalled at all.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# TWO OF THE FOUR CHECKS ASK ABOUT THIS MACHINE, and a hosted CI runner is not
# it. They are not skipped for convenience -- they have NO POWER there, and
# both of the answers they can give are wrong:
#
#   * A GitHub-hosted runner's home directory sits under `/home/` with the
#     service account as its last component, so `Path.home().name` is the
#     literal word that account is called. The check then searches every
#     tracked file for that word -- which .github/workflows/ci.yml contains
#     fourteen times, quite properly, being a document about those machines.
#     That is the FALSE FAIL that surfaced this.
#
#     (This comment does not spell the path out. Written with the literal
#     example in it, this file tripped its own home-path rule -- the same
#     lesson ALLOWED_PATHS already records twice, one level further in.)
#
#     The false PASS is the worse half and was there all along: on any runner
#     whose account name is generic, this check reports "ok" while never
#     having looked for the developer's name at all. A check that cannot fail
#     for the right reason is one this project deletes or fixes.
#
#   * `git config user.email` asks what identity the NEXT commit would carry.
#     A runner makes no commits and has none configured, so the check reports
#     a missing identity as a leak.
#
# Both still run, and must still pass, on a developer machine and in
# hooks/pre-catch -- which are the two gates that actually stand between the
# content and publication. What CI can genuinely check, it does: a
# home-directory-shaped path belonging to ANYONE, and the identity on every
# commit already in history.
#
# The condition is deliberately two-part, so setting CI=true on a real machine
# does not disable anything: the environment must say hosted CI AND the
# account name must be a service account rather than a person's.
SERVICE_ACCOUNTS = {"runner", "runneradmin", "root", "containeradmin",
                    "circleci", "jenkins", "buildkite-agent"}


def local_gate_reason():
    """-> why the machine-specific checks cannot run here, or None."""
    on_ci = (os.environ.get("GITHUB_ACTIONS") == "true"
             or os.environ.get("CI") == "true")
    if not on_ci:
        return None
    who = (Path.home().name or "").lower()
    if who not in SERVICE_ACCOUNTS:
        return None
    return ("hosted CI: the home account is the service account %r, so "
            "'this machine' is not a person's machine" % who)

# Identities allowed to appear as commit author or committer. Shapes, not
# addresses: anything routing to a real mailbox is rejected by not matching.
ALLOWED_EMAIL = re.compile(
    r"^(?:[0-9]+\+)?[A-Za-z0-9._-]+@users\.noreply\.github\.com$"
    r"|^noreply@[A-Za-z0-9.-]+$")

# A home directory in any tracked file. `<anything>` is the account name and
# is exactly what must not be published.
HOME_PATH = re.compile(
    r"(?:[A-Za-z]:[/\\]Users[/\\]|/home/|/Users/)([A-Za-z0-9._-]+)")

# Files where a home-shaped path is documentation rather than a real path.
# Each needs a reason; "it was already there" is not one.
ALLOWED_PATHS = {
    # Documents the rule "use forward slashes on Windows" and needs an
    # example path to do it. The account component is literally `...`.
    "SHELL-TRAPS.md",
    # The guard's own fixtures. It plants both home-path shapes to prove
    # this check FIRES on them, so those strings must exist inside it --
    # and without this entry the check refused the very commit that
    # introduced its own test. The account component there is a
    # placeholder, and the guard asserts that planting the same shapes into
    # a file that is NOT on this list still fails.
    #
    # Note this comment does not spell the shapes out. Written with the
    # literal example paths in it, this file tripped its own rule -- which
    # is the same lesson as the entry above, one level further in.
    "test_privacy_guard.py",
}

RESULTS = []
SKIPPED = []


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def not_here(name, why):
    """A check that cannot run in this environment. NOT a pass.

    Kept out of RESULTS entirely, so it can never be counted as evidence, and
    printed at the end where it cannot be skimmed past.
    """
    SKIPPED.append((name, why))
    print("  n/a  %s  -- %s" % (name, why))


def git(*args):
    r = subprocess.run(["git"] + list(args), cwd=str(ROOT),
                       capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def tracked():
    out = git("ls-files")
    return [l for l in (out or "").splitlines() if l.strip()]


def main():
    print("privacy -- nothing here names a person; the rules are derived")
    print("")

    local_only = local_gate_reason()

    # 1. The account name of whoever is running this, in any tracked file.
    account = Path.home().name
    hits = []
    if local_only:
        not_here("this machine's account name is not in any tracked file",
                 local_only)
    elif account and len(account) >= 3:
        pat = re.compile(re.escape(account), re.I)
        for rel in tracked():
            p = ROOT / rel
            try:
                text = p.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for i, line in enumerate(text.splitlines(), 1):
                if pat.search(line):
                    hits.append((rel, i, line.strip()[:70]))
    if not local_only:
        check("this machine's account name is not in any tracked file",
              not hits, "%d hit(s)" % len(hits) if hits else "")
        for rel, i, line in hits[:8]:
            print("       %s:%d  %s" % (rel, i, line))

    # 2. Any home-directory-shaped path, whoever it belongs to.
    hp = []
    for rel in tracked():
        if Path(rel).name in ALLOWED_PATHS:
            continue
        p = ROOT / rel
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for i, line in enumerate(text.splitlines(), 1):
            m = HOME_PATH.search(line)
            if m:
                hp.append((rel, i, line.strip()[:70]))
    check("no tracked file contains a home-directory path", not hp,
          "%d hit(s)" % len(hp) if hp else "")
    for rel, i, line in hp[:8]:
        print("       %s:%d  %s" % (rel, i, line))

    # 3. Every commit identity must be a privacy address.
    log = git("log", "--format=%an <%ae>%n%cn <%ce>")
    bad = set()
    if log is None:
        check("commit identities are privacy addresses", False,
              "git log unavailable")
    else:
        for line in log.splitlines():
            line = line.strip()
            if not line or "<" not in line:
                continue
            email = line[line.rindex("<") + 1:line.rindex(">")]
            if not ALLOWED_EMAIL.match(email):
                bad.add(email)
        check("every commit identity is a privacy address", not bad,
              ", ".join(sorted(bad)) if bad else
              "%d commit(s) checked" % len(log.splitlines()))

    # 4. And the identity git would use for the NEXT commit.
    nxt = (git("config", "user.email") or "").strip()
    if local_only and not nxt:
        # A runner makes no commits and configures no identity. An unset
        # value here is the absence of a question, not a failed answer.
        not_here("the configured commit email is a privacy address",
                 local_only + "; and no identity is configured")
    else:
        check("the configured commit email is a privacy address",
              bool(nxt) and bool(ALLOWED_EMAIL.match(nxt)),
              nxt or "user.email is unset")

    print("")
    bad_n = RESULTS.count(False)
    print("%d of %d applicable check(s) passed"
          % (len(RESULTS) - bad_n, len(RESULTS)))
    if SKIPPED:
        print("")
        print("%d CHECK(S) DID NOT RUN HERE and are not counted above:"
              % len(SKIPPED))
        for name, why in SKIPPED:
            print("  - %s" % name)
        print("They ask about the machine running them, and this is not a")
        print("developer's machine. They run -- and must pass -- in")
        print("tools/verify.py and hooks/pre-commit, which are the gates")
        print("between the content and publication. Do not read this run as")
        print("clearing what they cover.")
    if bad_n:
        print("")
        print("Publishing now would put the above into a public repository.")
        print("Content in a tracked file can simply be edited. An identity")
        print("already in COMMIT HISTORY needs a filter-branch, and once the")
        print("repository has been cloned it cannot be recalled at all.")
    return 1 if bad_n else 0


if __name__ == "__main__":
    sys.exit(main())
