"""permute.py's scorer must skip relocated words, not count them as wrong.

    python tools/test_permute.py

Six checks on `permute.score`, and the third is the one that matters: a
variant whose every non-relocated word is correct must score FULL MARKS even
though its relocated words differ from the image, because an object names
its symbols by placeholder and those words differ by construction.

Scoring them as mismatches is not merely pessimistic. It biases the ranking
toward shapes with FEWER relocations rather than shapes with better code, so
the tool recommends the wrong direction -- and it makes an exact match
unreportable for any function that calls anything. `sub_82287E80` matches
byte for byte and scored 38 of 40 under the old rule.

The synthetic inputs here are deliberate: a real compile would test the
compiler at the same time, and when a combined test fails it does not say
which half broke.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from permute import score

RESULTS = []


def check(name, got, want):
    ok = (got == want)
    RESULTS.append(ok)
    print("  %-4s %-52s got %s" % ("ok" if ok else "FAIL", name, (got,)))
    if not ok:
        print("       wanted %s" % (want,))


def words(*ws):
    return struct.pack(">%dI" % len(ws), *ws)


def mask(*flags):
    """One flag per WORD; expanded to the per-byte mask score() expects."""
    out = bytearray()
    for f in flags:
        out += bytes([1 if f else 0]) * 4
    return bytes(out)


def main():
    print("permute.score -- 6 checks (identical, compared, relocated)")
    print("")

    A, B, C, D = 0x11111111, 0x22222222, 0x33333333, 0x44444444

    # 1. Nothing relocated, everything right.
    check("all words match, none relocated",
          score(words(A, B, C), mask(1, 1, 1), words(A, B, C), 12),
          (3, 3, 0))

    # 2. Nothing relocated, one wrong.
    check("one word wrong, none relocated",
          score(words(A, D, C), mask(1, 1, 1), words(A, B, C), 12),
          (2, 3, 0))

    # 3. THE ONE THAT WAS WRONG. The relocated word differs -- as it always
    #    does, the object holding a placeholder -- and must not be compared.
    check("relocated word differs: NOT counted as a mismatch",
          score(words(A, 0, C), mask(1, 0, 1), words(A, B, C), 12),
          (2, 2, 1))

    # 4. And it must not be counted as a match either, if it happens to be
    #    equal by luck. A word that cannot be compared is not evidence.
    check("relocated word equal: still not compared",
          score(words(A, B, C), mask(1, 0, 1), words(A, B, C), 12),
          (2, 2, 1))

    # 5. Every word relocated -- nothing was verified, and the caller has to
    #    be able to tell that from a perfect score.
    check("every word relocated: zero compared",
          score(words(0, 0), mask(0, 0), words(A, B), 8),
          (0, 0, 2))

    # 6. Ours shorter than the target: compare only the overlap.
    check("ours shorter than target",
          score(words(A, B), mask(1, 1), words(A, B, C), 12),
          (2, 2, 0))

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    if bad:
        print("")
        print("permute.py's ranking cannot be trusted: it is scoring the")
        print("relocation mask rather than the code.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
