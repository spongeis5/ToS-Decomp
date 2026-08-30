"""Where the jump tables are. ONE reader for build/switch_tables.txt.

    python tools/switchtab.py         the ranges, and what disagreed before

A jump table is DATA sitting in `.text`. It is preceded by the `bctr` that
reads it, and the code that builds its base does so with the same `lis`/
`addi` pair a function pointer uses, so it looks like a function start from
every cheap angle. Five tools therefore need to exclude them, and until this
existed all five read the file themselves. They did not agree:

  * `bridge.py` parsed the length as HEXADECIMAL. The file writes decimal, so
    `94` became 0x94 and `880` became 2176 -- 329 of 437 entries over-read,
    24,714 bytes over-excluded. bridge.py is what ranks the next function to
    work on, so it had been quietly dropping candidates that sit after a
    table.
  * `match.py` skipped any entry whose length is 0, so those tables were not
    excluded at all.
  * `interior.py` treated a length of 0 as a LENGTH, making `lo <= a < lo+0`
    an empty range -- 96 jump tables reported as hidden functions, 54,876
    bytes.
  * `addrtaken.py` excluded switch case BODIES and never table BASES at all:
    331 of its 1,252 addresses were tables.
  * `truncated.py` was the only one that parsed the length correctly, and it
    still read 0 as empty.

**A LENGTH OF 0 MEANS "COULD NOT BE RECOVERED", NOT "EMPTY".**
`switch_tables.txt` says so in its own header -- "bytes is 0 where the case
count could not be recovered" -- and 106 of 437 entries are in that state.
Reading it as a length is the failure this project names first: absence of
evidence rendered as evidence of absence.

So an unknown extent is MEASURED rather than assumed away. A table of the
absolute-address form -- the most common of the three, per FINDINGS 7v -- is
a run of aligned words that are all addresses inside `.text`, so the run
itself gives the extent. Where even the first word is not such an address the
run is 0 and nothing is excluded, which is no worse than before: the fallback
never claims more than it can see.
"""

import bisect
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image

ROOT = Path(__file__).resolve().parent.parent
FILE = ROOT / "build/switch_tables.txt"
RUN_LIMIT = 4096


def address_run(img, a0, limit=RUN_LIMIT):
    """How many bytes at `a0` are a run of aligned `.text` addresses."""
    text = next((s for s in img.sections if s["name"] == ".text"), None)
    if text is None:
        return 0
    lo = text["va"]
    hi = lo + (text["vsize"] or text["rawsz"])
    n = 0
    while n < limit:
        b = img.read(a0 + n, 4)
        if b is None:
            break
        w = struct.unpack(">I", b)[0]
        if w % 4 or not (lo <= w < hi):
            break
        n += 4
    return n


class Tables(object):
    """The jump-table ranges. `va in tables` is the whole interface.

    `missing` is True when build/switch_tables.txt is absent -- which every
    caller should REPORT rather than treat as "no tables", because the two
    are the same object with very different meanings.
    """

    def __init__(self, img, path=FILE):
        self.ranges = []
        self.unknown = 0
        self.missing = not Path(path).exists()
        if self.missing:
            self._lo = []
            return
        for line in Path(path).read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) < 2:
                continue
            try:
                a0 = int(f[0], 16)
                # DECIMAL. The producer writes decimal and one consumer read
                # it as hex for long enough to over-exclude 24,714 bytes.
                n = int(f[1], 10)
            except ValueError:
                continue
            if n == 0:
                self.unknown += 1
                n = address_run(img, a0)
            if n:
                self.ranges.append((a0, a0 + n))
        self.ranges.sort()
        self._lo = [lo for lo, _hi in self.ranges]

    def __contains__(self, va):
        i = bisect.bisect_right(self._lo, va) - 1
        return i >= 0 and va < self.ranges[i][1]

    def __len__(self):
        return len(self.ranges)

    def total_bytes(self):
        return sum(hi - lo for lo, hi in self.ranges)


def main(argv):
    img = Image()
    t = Tables(img)
    if t.missing:
        print("build/switch_tables.txt is missing -- run tools/switches.py.")
        print("Refusing to report zero jump tables, which is what an empty")
        print("read would look like and is not the same statement.")
        return 1
    print("%d jump table(s), %d byte(s) of .text that are DATA"
          % (len(t), t.total_bytes()))
    print("%d of them record NO LENGTH; extent measured as the run of"
          % t.unknown)
    print("aligned .text addresses at the base.")
    print("")
    print("Read as a length of 0 those %d exclude nothing, which is what"
          % t.unknown)
    print("interior.py and match.py each did, in their own way.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
