"""Run match.py over every attempts.txt row at both levels; dump to a log.

Scratch helper for the near-miss session. Not part of the build.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
rows = []
for line in (ROOT / "src/attempts.txt").read_text().splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    f = line.split()
    rows.append((f[0], f[1]))

only = sys.argv[1:]
for src, addr in rows:
    if only and not any(o.lower() in src.lower() or o.lower() == addr.lower()
                        for o in only):
        continue
    for lname, lflags in (("/O2", None), ("/O2 /Os", "/O2,/Os,/Gy,/GS-,/fp:fast")):
        cmd = [sys.executable, "tools/match.py", src, addr]
        if lflags:
            cmd += ["--flags", lflags]
        print("=" * 72)
        print("### %s  %s  [%s]" % (src, addr, lname))
        sys.stdout.flush()
        r = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)
        print(r.stdout)
        if r.stderr.strip():
            print("STDERR:", r.stderr[:2000])
        sys.stdout.flush()
