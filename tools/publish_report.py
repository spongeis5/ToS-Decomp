"""Regenerate everything decomp.dev reads, and put report.bin at the root.

    python tools/publish_report.py

decomp.dev consumes a progress report uploaded by CI as an artifact named
`default_report`. That report CANNOT be built on a GitHub-hosted runner:
generating it needs the retail image and the XDK's cl.exe, and neither is
in this repository or should be.

So the report is generated here, on a machine that has both, and committed.
The workflow re-uploads it. Its figures are then as fresh as the last time
someone ran this -- which is honest as long as it is said, and the commit
message this prints says it.

Protobuf, not JSON, and the difference is not small: 898 KB against 2.8 MB
compact or 5.2 MB pretty. Git keeps every version of a committed file
forever, so the format choice is a decision about repository size for the
life of the project rather than a preference.

`objdiff-cli` is not vendored -- 7 MB does not belong in git history. Put it
in `tools/bin/` (gitignored) from
https://github.com/encounter/objdiff/releases
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "report.bin"

CANDIDATES = [
    ROOT / "tools/bin/objdiff-cli.exe",
    ROOT / "tools/bin/objdiff-cli",
]


def run(args, **kw):
    r = subprocess.run(args, cwd=str(ROOT), **kw)
    if r.returncode != 0:
        print("")
        print("FAILED: %s" % " ".join(str(a) for a in args))
        sys.exit(r.returncode)
    return r


def main():
    cli = next((c for c in CANDIDATES if c.exists()), None)
    if cli is None:
        print("objdiff-cli not found. Looked in:")
        for c in CANDIDATES:
            print("    %s" % c)
        print("")
        print("Download it from https://github.com/encounter/objdiff/releases")
        print("and put it there. It is gitignored on purpose: a 7 MB binary")
        print("does not belong in git history.")
        return 1

    if not (ROOT / "build/default.pe.exe").exists():
        print("build/default.pe.exe is missing -- the retail image is needed")
        print("to build the target objects the report measures.")
        return 1

    print("1/3  coverage objects for the code with no source yet")
    run([sys.executable, "tools/coverage.py", "--write"])

    print("2/3  objdiff unit list")
    run([sys.executable, "tools/objdiff_export.py"],
        stdout=subprocess.DEVNULL)

    print("3/3  progress report")
    run([str(cli), "report", "generate", "-p", ".", "-o", str(OUT),
         "-f", "proto"])

    size = OUT.stat().st_size
    print("")
    print("wrote %s (%.0f KB)" % (OUT, size / 1024.0))
    print("")
    print("COMMIT IT. The workflow uploads this file as the `default_report`")
    print("artifact, and decomp.dev reads that. Its figures are as fresh as")
    print("this run -- worth saying in the commit message rather than")
    print("implying CI measured them:")
    print("")
    print("    git add report.bin")
    print('    git commit -m "Progress report: <N> matched, <B> bytes"')
    return 0


if __name__ == "__main__":
    sys.exit(main())
