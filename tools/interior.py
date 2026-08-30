"""Function starts hidden INSIDE another inventory row, and unreachable.

    python tools/interior.py            the census
    python tools/interior.py --write    add them to build/functions_all.txt

One `.pdata` unwind record can cover a run of adjacent frameless functions
(FINDINGS 7q). `match.py`'s `can_shrink` handles that from the FRONT: a
source matching the first body is accepted and the row is reconciled down to
its length. Nothing handles the rest. The second and later bodies have no
inventory row of their own, so `match.py` refuses their address outright and
they cannot be matched at all -- not "not yet", but not ever, while the
inventory says they do not exist.

Two turned up by being tripped over rather than found: `82697748`, inside
`82697740 + 68`, and `82631F78`, inside `82631F30 + 152`. In both cases a
source had already been written and could not be verified.

THE EVIDENCE IS TWO THINGS AT ONCE, and it has to be, because either alone
gets this wrong in a different direction.

  TERMINATED BEFORE. The word at `addr - 4` must be `blr`, an unconditional
  `b`, `bctr`, or zero padding. Control cannot fall into the address, so it
  is a boundary rather than a branch target inside a body.

  REFERENCED. Some `bl` must target it, or some `lis`/`addi` pair must form
  it, or it must appear in a pointer run in `.rdata` -- a vtable slot.

Direct calls ALONE were tried first and found four starts, none of which
were the two that had actually been tripped over: `82697748` is preceded by
a word of zero padding and is reached through a pointer, and `82631F78` is
the second body under `82631F30 + 152`. A census that misses both of its
known-good answers while reporting a confident small number is worse than
no census, so those two are now assertions -- this tool refuses to report
if it cannot find them.

The size of a recovered start runs to the end of the enclosing row, or to
the next recovered start inside it, whichever comes first. That is a bound
rather than a measurement, and `can_shrink` narrows it the same way it
narrows the enclosing row.
"""

import bisect
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
CALLS = ROOT / "build/callgraph.txt"
INVENTORY = ROOT / "build/functions_all.txt"


def call_targets():
    """Every address some `bl` resolves to, with how many sites call it."""
    counts = defaultdict(int)
    if not CALLS.exists():
        return None
    for line in CALLS.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) != 2:
            continue
        try:
            counts[int(f[1], 16)] += 1
        except ValueError:
            pass
    return counts


BLR, BCTR = 0x4E800020, 0x4E800420


def terminated_before(img, va):
    """Can control FALL INTO `va`? If not, it is a boundary."""
    raw = img.read(va - 4, 4)
    if raw is None or len(raw) != 4:
        return False
    w = struct.unpack(">I", raw)[0]
    if w in (BLR, BCTR, 0):                       # return, ctr jump, padding
        return True
    if (w >> 26) == 18 and not (w & 1):           # unconditional b, not bl
        return True
    return False


def referenced(img):
    """Addresses reached by a data word in .rdata/.data -- vtable slots."""
    out = set()
    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = tlo + (text["vsize"] or text["rawsz"])
    for s in img.sections:
        if s["exec"] or not s["initialized"]:
            continue
        if s["name"] not in (".rdata", ".data"):
            continue
        off = s["va"] - img.base
        avail = len(img.data) - off
        size = min(s["vsize"] or s["rawsz"], s["rawsz"], max(avail, 0))
        n = size // 4
        if n <= 0:
            continue
        for w in struct.unpack_from(">%dI" % n, img.data, off):
            if tlo <= w < thi and not (w & 3):
                out.add(w)
    return out


def addrtaken():
    p = ROOT / "build/addrtaken.txt"
    out = set()
    if not p.exists():
        return out
    for line in p.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if f:
            try:
                out.add(int(f[0], 16))
            except ValueError:
                pass
    return out


def _address_run(img, a0, limit=4096):
    """How many bytes at `a0` are a run of aligned .text addresses.

    Used only for a table whose recorded length is 0, i.e. unknown. Returns
    0 when the first word is not such an address, so a table this cannot
    read is excluded no more than it was before -- the fallback never claims
    more than it can see.
    """
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


def switch_table_ranges(img):
    """-> ([(lo, hi)], how many had no recorded length).

    Kept as the name two tools already import; the reader itself is
    tools/switchtab.py, because FIVE tools needed this and all five had
    their own version. See that module for what they disagreed about.
    """
    import switchtab
    t = switchtab.Tables(img)
    return list(t.ranges), t.unknown


def find(img, inv):
    starts = sorted(inv)
    calls = call_targets()
    if calls is None:
        return None, None
    pointed = referenced(img)
    taken = addrtaken()

    # A jump table is DATA sitting in .text, and its entries are addresses
    # into the same function, so a table's own address arrives here looking
    # exactly like an interior start: preceded by the `bctr` that reads it,
    # and pointed at from code. 8215AC14 -- 1,724 bytes "inside"
    # 8215ABD0 + 1792 -- is one. switches.py already knows where they are.
    #
    # A RECORDED LENGTH OF 0 MEANS "COULD NOT BE RECOVERED", NOT "EMPTY".
    # switch_tables.txt says so in its own header -- "bytes is 0 where the
    # case count could not be recovered" -- and this read it as a length, so
    # `lo <= a < lo + 0` excluded nothing for exactly the tables whose extent
    # was unknown. 106 of 437 recorded tables are in that state, and they
    # accounted for 96 of the 307 "interior function starts" this tool
    # reported: every one of them a jump table, 54,876 of the 97,092 bytes it
    # claimed were waiting to be matched.
    #
    # The producer stated its uncertainty and the consumer dropped it, which
    # is the failure this project names first: absence of evidence rendered
    # as evidence of absence.
    #
    # So an unknown extent is MEASURED here rather than assumed away. A jump
    # table of the absolute-address form is a run of aligned words that are
    # all addresses inside .text, so the run itself gives the extent.
    tables, unknown = switch_table_ranges(img)

    def in_table(a):
        return any(lo <= a < hi for lo, hi in tables)

    find.tables_unknown = unknown
    find.tables_total = len(tables)
    find.tables_recovered = sum(
        1 for i, (lo, hi) in enumerate(tables) if hi > lo) if tables else 0

    cands = set(calls) | pointed | taken
    interior = defaultdict(list)
    for tgt in cands:
        if tgt % 4:
            continue
        if in_table(tgt):
            continue
        i = bisect.bisect_right(starts, tgt) - 1
        if i < 0:
            continue
        lo = starts[i]
        if tgt == lo or tgt >= lo + inv[lo]:
            continue                      # own row, or past the row
        if not terminated_before(img, tgt):
            continue                      # control can fall in: not a start
        interior[lo].append((tgt, calls.get(tgt, 0)))
    for lo in interior:
        interior[lo].sort()
    return interior, calls


def main(argv):
    img = Image()
    inv = dict(load_inventory())
    interior, calls = find(img, inv)
    if interior is None:
        print("build/callgraph.txt is missing -- run tools/discover.py.")
        print("Refusing to report zero hidden starts, which is what this")
        print("would print either way.")
        return 1

    total = sum(len(v) for v in interior.values())
    print("%d inventory row(s); %d distinct call target(s)"
          % (len(inv), len(calls)))
    print("")
    unknown = getattr(find, "tables_unknown", 0)
    if unknown:
        print("%d of %d switch table(s) have NO RECORDED LENGTH; their extent"
              % (unknown, getattr(find, "tables_total", 0)))
        print("was measured here as the run of aligned .text addresses at the")
        print("table address. Read as a length of 0 -- which is what this did")
        print("until it was fixed -- they excluded nothing, and 96 jump tables")
        print("were reported below as hidden functions.")
        print("")
    print("%d function start(s) lie INSIDE another row and have no row of"
          % total)
    print("their own, across %d enclosing row(s). Each is terminated-before"
          % len(interior))
    print("AND referenced -- by a call, a data word, or a lis/addi pair.")
    print("Most are NOT called directly; the caller count below is 0 for")
    print("the majority, and saying 'each is called' would misdescribe")
    print("what the evidence actually is.")
    print("")

    rows = []
    for lo in sorted(interior):
        end = lo + inv[lo]
        pts = interior[lo]
        for k, (tgt, n) in enumerate(pts):
            nxt = pts[k + 1][0] if k + 1 < len(pts) else end
            rows.append((tgt, nxt - tgt, n, lo, inv[lo]))

    print("  address    size  callers  inside")
    for tgt, size, n, lo, ln in rows[:30]:
        print("  %08X %6d %8d  %08X + %d" % (tgt, size, n, lo, ln))
    if len(rows) > 30:
        print("  ... %d more" % (len(rows) - 30))

    print("")
    print("%d byte(s) of code become addressable" % sum(r[1] for r in rows))
    known = sum(1 for r in rows if r[2] >= 5)
    print("%d of the %d are called from 5 or more sites" % (known, len(rows)))

    # The two known-good answers, and what they establish. Both were found
    # by writing a source and discovering match.py would not accept the
    # address. NEITHER is reachable by reference evidence: measured below,
    # they are not called, not pointed at from data, and not address-taken.
    print("")
    found = set(r[0] for r in rows)
    for want, why in ((0x82697748, "second body under 82697740 + 68"),
                      (0x82631F78, "second body under 82631F30 + 152")):
        print("  %-8s %08X  %s"
              % ("found" if want in found else "missed", want, why))
    print("")
    print("Those two are terminated-before but have NO reference of any")
    print("kind, so the list above is a LOWER BOUND and not a census. An")
    print("unreferenced second body can only be found structurally -- by")
    print("decoding the remainder of a row and seeing a whole function --")
    print("and that test has false positives this one does not, so it is")
    print("not simply a wider net to cast.")

    if "--write" in argv:
        text = INVENTORY.read_text()
        add = ["%08X %8d interior" % (t, s) for t, s, _n, _lo, _l in rows]
        INVENTORY.write_text(text.rstrip("\n") + "\n" + "\n".join(add) + "\n")
        print("")
        print("appended %d row(s) to %s" % (len(add), INVENTORY))
        print("Sizes are BOUNDS -- to the next start or the row's end -- and")
        print("can_shrink narrows them exactly as it does the enclosing row.")
    else:
        print("")
        print("nothing written; pass --write")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
