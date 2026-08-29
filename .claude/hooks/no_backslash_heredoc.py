"""Refuse Bash commands that put a backslash inside a heredoc.

This trap corrupted files FOUR times in the session that created this project,
twice silently:

  * a regex `rb'[A-Za-z]:\\[...]'` arrived as `\[`, searched for a literal
    bracket, and returned ZERO Windows paths from an image containing 64 --
    no error, just a benign-looking empty result
  * `b"\\x60\\x00\\x00\\x00"` arrived as real control bytes and were written
    into a generated .py, which then died with "source code cannot contain
    null bytes" and could not be repaired by an ordinary text edit
  * `.replace('\\','/')` arrived as `.replace('\','/')`  -- SyntaxError
  * `"...%d\\n"` arrived as a literal newline inside a string literal, twice

`<<'EOF'` is supposed to be fully literal and is not behaving that way here;
see SHELL-TRAPS.md. Because the behaviour is inconsistent, "count the
backslashes carefully" is not a workable defence -- so this refuses the
construct outright.

The fix is always the same: write the script to a file with the Write tool and
run it, or avoid the backslash (chr(92), bytes([0x60, ...]), forward slashes).

Exit 2 blocks the call and shows stderr to Claude.
"""

import json
import re
import sys

BS = chr(92)


def main():
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return 0                      # never block on a malformed payload

    if payload.get("tool_name") != "Bash":
        return 0
    cmd = (payload.get("tool_input") or {}).get("command", "")
    if not cmd or "<<" not in cmd or BS not in cmd:
        return 0

    # Find each heredoc body and check only that, so a backslash elsewhere in
    # the command line (a Windows path in an argument, say) is not blocked.
    offenders = []
    for m in re.finditer(r"<<-?\s*'?\"?([A-Za-z_][A-Za-z_0-9]*)'?\"?", cmd):
        tag = m.group(1)
        body_start = cmd.find("\n", m.end())
        if body_start < 0:
            continue
        end = re.search(r"^\s*" + re.escape(tag) + r"\s*$",
                        cmd[body_start:], re.M)
        body = cmd[body_start:body_start + end.start()] if end else cmd[body_start:]
        for i, line in enumerate(body.splitlines(), 1):
            if BS in line:
                offenders.append((tag, i, line.strip()[:100]))

    if not offenders:
        return 0

    print("BLOCKED: backslash inside a heredoc.", file=sys.stderr)
    print("", file=sys.stderr)
    for tag, i, line in offenders[:6]:
        print("  <<%s line %d:  %s" % (tag, i, line), file=sys.stderr)
    if len(offenders) > 6:
        print("  ... and %d more" % (len(offenders) - 6), file=sys.stderr)
    print("", file=sys.stderr)
    print("Heredocs in this shell strip a backslash level INCONSISTENTLY, so the",
          file=sys.stderr)
    print("bytes that arrive are not the bytes written. This has silently",
          file=sys.stderr)
    print("corrupted files here four times. See SHELL-TRAPS.md.", file=sys.stderr)
    print("", file=sys.stderr)
    print("Instead:", file=sys.stderr)
    print("  * write the script to a file with the Write tool, then run it", file=sys.stderr)
    print("  * or avoid the backslash: chr(92), bytes([0x60,0,0,0]), '/' paths",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
