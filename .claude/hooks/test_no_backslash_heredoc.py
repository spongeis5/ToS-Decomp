"""Verify the heredoc guard fires on a bad command and NOT on good ones.

A guard that cannot be shown to fire is indistinguishable from no guard, and
one that fires on correct input teaches you to reach past guards. Both
directions are checked here.
"""

import json
import subprocess
import sys
from pathlib import Path

HOOK = Path(__file__).with_name("no_backslash_heredoc.py")
BS = chr(92)

CASES = [
    # (name, command, expect_blocked)
    ("heredoc with a backslash escape",
     "python <<'PY'\ns = '" + BS + BS + "'\nPY\n", True),
    ("heredoc with a byte escape",
     'python <<' + "'PY'\nNOP = b\"" + BS + "x60\"\nPY\n", True),
    ("heredoc with a regex escape",
     "python <<'PY'\nimport re\nre.compile('" + BS + "d+')\nPY\n", True),
    ("clean heredoc, no backslash",
     "python <<'PY'\nprint(chr(92))\nPY\n", False),
    ("backslash OUTSIDE any heredoc",
     'ls "C:' + BS + 'Users' + BS + 'someone"', False),
    ("no heredoc at all",
     "python tools/pdata.py", False),
    ("heredoc, backslash only after the terminator",
     "python <<'PY'\nprint(1)\nPY\necho C:" + BS + "tmp\n", False),
]


def run(cmd):
    payload = json.dumps({"tool_name": "Bash", "tool_input": {"command": cmd}})
    r = subprocess.run([sys.executable, str(HOOK)], input=payload,
                       capture_output=True, text=True)
    return r.returncode, r.stderr


def main():
    ok = bad = 0
    for name, cmd, expect in CASES:
        code, err = run(cmd)
        blocked = (code == 2)
        good = (blocked == expect)
        print("  %-42s expect %-7s got %-7s  %s"
              % (name, "BLOCK" if expect else "allow",
                 "BLOCK" if blocked else "allow",
                 "ok" if good else "*** WRONG ***"))
        if good:
            ok += 1
        else:
            bad += 1
            if err:
                print("      stderr: %s" % err.splitlines()[0])
    print()
    print("  %d of %d correct" % (ok, ok + bad))
    if bad:
        print("  THE GUARD IS NOT TRUSTWORTHY -- fix it before relying on it.")
        return 1
    print("  Guard fires on every bad case and on no good case.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
