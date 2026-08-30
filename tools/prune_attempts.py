"""Drop near-miss rows whose address is now matched.

    python tools/prune_attempts.py           what it would drop
    python tools/prune_attempts.py --write    drop them

`verify.py` refuses a tree where one address is both matched and a near-miss.
That check exists because report.py reads BOTH files, so such an address
becomes two units and is counted twice, and because `src/attempts.txt` then
overstates what still resists. This is the tool that fixes what it detects; a
check with no remedy gets worked around.

It happens two ways and both are ordinary:

  * a near-miss is SOLVED, and the same source file moves to the manifest
    while its old attempts row stays behind. Four of those landed in one
    afternoon when the arena twins, `g5_chunked_bit` and `m36_adjust_counts`
    all fell.
  * a DIFFERENT source matches the address first. `825409E8` was
    `l45_cmp_bytes_n.cpp` at 13 of 16 and was matched by `z1_memcmp_n.cpp`.
    `sweep.py --attempts --check` cannot see that one: it fires when an
    attempt's OWN source starts matching, and l45's still does not.

BYTES, not text. Reading a file as text and writing it back translates line
endings on Windows, which rewrites every line and has twice cost an hour here
-- once corrupting the privacy guard's victim file, once invalidating
`build/linked.txt` by changing the manifest's digest without changing a word
of it.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "src/manifest.txt"
ATTEMPTS = ROOT / "src/attempts.txt"


def addresses(path):
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.split("#")[0].strip()
        if not s:
            continue
        f = s.split()
        if len(f) >= 2:
            try:
                out[int(f[1], 16)] = f[0]
            except ValueError:
                pass
    return out


def main(argv):
    matched = addresses(MANIFEST)
    raw = ATTEMPTS.read_bytes()
    nl = b"\r\n" if b"\r\n" in raw else b"\n"
    lines = raw.split(nl)

    keep, dropped = [], []
    for b in lines:
        s = b.decode("utf-8", "replace").split("#")[0].strip()
        f = s.split()
        if len(f) >= 2:
            try:
                a = int(f[1], 16)
            except ValueError:
                a = None
            if a is not None and a in matched:
                dropped.append((a, f[0], matched[a]))
                continue
        keep.append(b)

    print("%d attempt row(s) name an address that is now matched:"
          % len(dropped))
    for a, src, by in dropped:
        print("  %08X  %-34s matched by %s"
              % (a, src, "the same file" if src == by else by))
    if not dropped:
        print("  none -- nothing to do")
        return 0
    if "--write" not in argv:
        print("")
        print("nothing written; pass --write")
        return 0
    ATTEMPTS.write_bytes(nl.join(keep))
    print("")
    print("dropped %d of %d line(s) from %s"
          % (len(dropped), len(lines), ATTEMPTS.relative_to(ROOT).as_posix()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
