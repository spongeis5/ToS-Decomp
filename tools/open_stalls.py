"""The list of open stalls must not name a function that already matches.

    python tools/open_stalls.py            list them with their live state
    python tools/open_stalls.py --check    fail if one of them is matched

MATCHED.md's "What still resists" carried this sentence for two revisions:

    `sub_8216C240`'s last word is an `or` operand order, which sixteen
    spellings and 72 flag combinations show is not source-readable.

Every clause of that was true when it was measured. The function came out at
38 of 38 anyway -- the sixteen spellings had all been spellings of the
OPERATOR, and the lever was to mask the INDEX. Meanwhile the same document's
generated table, three hundred lines earlier, had been listing `8216C240` as
matched the whole time.

A stale figure is embarrassing. A stale STALL is expensive, because its
only readers are the people deciding what to work on next, and it tells
them not to. That section's own opening paragraph says so -- "a stale list
of stalls does not merely go out of date; it tells the next reader not to
try" -- and it went stale anyway, four revisions running.

So the list lives in ONE place, HANDBOOK.md's "Still genuinely open"
bullets, and this checks it against the manifest.

WHY THIS IS THE ONLY REGION CHECKED. The prose around it deliberately names
functions that DID come out -- that is the useful half of the section, and
`8216C240`, `82806FD0` and `82600AD0` all appear there as worked examples of
wrong explanations. A check over the whole section would fire on correct
input, which is worse than no check: it teaches you to reach past checks.
The bullets are delimited, so they can be read exactly.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "HANDBOOK.md"
BEGIN = "**Still genuinely open, and the honest reasons:**"
END = "**Larger, in rough order of value:**"
ADDR = re.compile(r"`(8[0-9A-F]{7})`")


def rows(path):
    """{address: source} from a manifest-shaped file."""
    out = {}
    p = ROOT / path
    if not p.exists():
        return out
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        f = line.split()
        if len(f) >= 2:
            try:
                out[int(f[1], 16)] = f[0]
            except ValueError:
                pass
    return out


def region():
    """-> (text, None) or (None, why not).

    A MISSING MARKER IS A FAILURE, NOT AN EMPTY LIST. readme_stats.py had
    exactly this hole: it substituted only when its anchor was found, so
    deleting the anchor silenced the check that anchor existed for.
    """
    doc = DOC.read_text(encoding="utf-8")
    if BEGIN not in doc:
        return None, "HANDBOOK.md has no %r heading" % BEGIN
    if END not in doc:
        return None, "HANDBOOK.md has no %r heading" % END
    i = doc.index(BEGIN)
    j = doc.index(END, i)
    if j <= i:
        return None, "the two headings are in the wrong order"
    return doc[i + len(BEGIN):j], None


def main(argv):
    text, why = region()
    if text is None:
        print("CANNOT READ THE OPEN-STALL LIST: %s" % why)
        print("")
        print("This is a failure, not a no-op. Without the markers there is")
        print("no list to check, and a check with nothing to check reports")
        print("success for the same reason a working one does.")
        return 1

    named = []
    for m in ADDR.finditer(text):
        a = int(m.group(1), 16)
        if a not in named:
            named.append(a)

    if not named:
        print("THE OPEN-STALL LIST NAMES NO ADDRESSES.")
        print("")
        print("Either every stall was solved -- in which case say so in")
        print("prose and delete the section -- or the bullets stopped using")
        print("the `%s` form this reads. An empty list is not a pass."
              % "8XXXXXXX")
        return 1

    matched = rows("src/manifest.txt")
    attempts = rows("src/attempts.txt")

    stale = [a for a in named if a in matched]
    unknown = [a for a in named if a not in matched and a not in attempts]

    for a in named:
        if a in matched:
            state = "MATCHED by %s" % matched[a]
        elif a in attempts:
            state = "near-miss, %s" % attempts[a]
        else:
            state = "in neither manifest nor attempts"
        print("  %08X  %s" % (a, state))
    print("")
    print("%d address(es) named as still open, of %d near-miss row(s)"
          % (len(named), len(attempts)))

    if stale:
        print("")
        print("%d OF THEM ALREADY MATCH:" % len(stale))
        for a in stale:
            print("    %08X  matched by %s" % (a, matched[a]))
        print("")
        print("Remove it from the list. A stall that has fallen and is still")
        print("written down as unreachable is the one kind of stale fact")
        print("that changes what the next reader does: it tells them not to")
        print("try. Every one of the levers in MATCHED.md was found by")
        print("someone trying anyway.")
    if unknown:
        print("")
        print("%d address(es) are in NEITHER src/manifest.txt nor" % len(unknown))
        print("src/attempts.txt, so nothing tracks their state:")
        for a in unknown:
            print("    %08X" % a)
        print("")
        print("Either add the row or stop naming the address here; an")
        print("address no file tracks cannot go stale loudly, only quietly.")

    bad = bool(stale or unknown)
    if "--check" in argv:
        print("")
        print("open-stall list is %s"
              % ("STALE" if bad else "consistent with the manifest"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
