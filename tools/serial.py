"""Adjacent-dependency density of a function. DOES NOT PREDICT MATCHABILITY.

    python tools/serial.py --validate      score the 10 known outcomes
    python tools/serial.py 821636A8 ...    score specific addresses
    python tools/serial.py                 rank the candidate list

**Read the refutation before using this.** It was written to encode what
looked like the lesson of five first-attempt matches: that a function whose
instructions each consume the previous one's result has one legal ordering,
so correct semantics give correct bytes. Validated against every outcome this
project has -- six matched functions and four stalled ones -- it fails:

    822607F0   MATCHED        serial 0.03    <- lowest score of all ten
    827C5198   stalled 3/5    serial 0.75    <- second highest

The reason is that the metric measures the opposite of what it was meant to.
An OPTIMISING compiler deliberately separates dependent instructions to hide
latency, so well-scheduled code has LOW adjacent dependency by construction.
A low score means the scheduler worked, not that it had freedom.

What actually distinguishes the four stalls is that in each one the compiler
made a free choice the source cannot express -- instruction order (826C1480),
branch polarity (82806FD0), or register assignment (827C5198, 8215E5B0).
That is a real distinction, but it is DIAGNOSTIC AFTER THE FACT, not
predictive, and this tool does not measure it.

Kept because `--validate` is the record of the refutation, and because the
ranking may still be a weak hint. It is not a filter. `--validate` prints its
own verdict every time rather than leaving the caller to remember this.

THE METRIC: the fraction of instructions that use a register the IMMEDIATELY
PRECEDING instruction defined. Read off disassembly text, not a real
dependency graph.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
import ppcdis

REG = re.compile(r"\b([rf])(\d{1,2})\b")
SPECIAL = re.compile(r"\b(ctr|lr|cr\d?)\b")

# The six matched and the four that resist, with what happened to each.
KNOWN = [
    (0x821636A8, "MATCHED", "five dependent loads"),
    (0x82540750, "MATCHED", "byte copy loop"),
    (0x8253FD70, "MATCHED", "guarded accumulate"),
    (0x822D2450, "MATCHED", "global array field address"),
    (0x82807B38, "MATCHED", "guarded tail call"),
    (0x822607F0, "MATCHED", "grid strip indices"),
    (0x826C1480, "stalled 13/19", "12 independent stores"),
    (0x82806FD0, "stalled 11/21", "chunked accessor"),
    (0x827C5198, "stalled 3/5", "virtual call, regalloc"),
    (0x8215E5B0, "stalled 1/7", "argument reshuffle"),
]


def regs(text):
    out = set()
    for kind, num in REG.findall(text):
        out.add(kind + num)
    for s in SPECIAL.findall(text):
        out.add(s)
    return out


def def_use(mnem, ops):
    """-> (defs, uses). Crude, and only has to be good enough to rank."""
    parts = [p.strip() for p in ops.split(",")] if ops else []
    all_regs = regs(ops)
    if not parts:
        return set(), all_regs

    first = regs(parts[0])
    rest = set()
    for p in parts[1:]:
        rest |= regs(p)

    if mnem.startswith("st"):
        # A store defines nothing, except an update form's base register.
        base = set()
        if mnem.endswith("u") or mnem.endswith("ux"):
            for p in parts[1:]:
                m = re.search(r"\(([rf]\d{1,2})\)", p)
                if m:
                    base.add(m.group(1))
                    break
        return base, all_regs
    if mnem.startswith("mt"):
        return SPECIAL.findall(mnem) and set() or {"ctr" if "ctr" in mnem
                                                   else "lr"}, all_regs
    if mnem.startswith("b"):
        return set(), all_regs
    if mnem.startswith("cmp"):
        return first, rest

    defs = set(first)
    uses = set(rest)
    # Update-form loads also write the base register.
    if mnem.endswith("u") or mnem.endswith("ux"):
        for p in parts[1:]:
            m = re.search(r"\(([rf]\d{1,2})\)", p)
            if m:
                defs.add(m.group(1))
    return defs, uses


def serial_score(img, va, size):
    """-> (score, ninstr) or (None, 0) if it could not be read."""
    data = img.read(va, size)
    if data is None or len(data) < size or size < 8:
        return None, 0
    ws = [int.from_bytes(data[i:i + 4], "big") for i in range(0, size, 4)]
    try:
        lines = ppcdis.words(ws, va)
    except Exception:
        return None, 0

    du = []
    for _a, _w, text in lines:
        text = text.strip()
        if not text or text.startswith("."):
            return None, 0
        bits = text.split(None, 1)
        mnem = bits[0]
        ops = bits[1] if len(bits) > 1 else ""
        du.append(def_use(mnem, ops))

    n = len(du)
    if n < 2:
        return None, 0
    chained = 0
    for i in range(1, n):
        if du[i][1] & du[i - 1][0]:
            chained += 1
    return chained / float(n - 1), n


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    img = Image()
    inv = dict(load_inventory())

    if "--validate" in argv[1:]:
        print("Scoring the ten functions whose outcome is known.\n")
        print("  %-10s %-16s %-7s %-6s %s"
              % ("address", "outcome", "serial", "instr", "what it is"))
        rows = []
        for va, outcome, what in KNOWN:
            size = inv.get(va)
            if size is None:
                print("  %08X  NOT IN INVENTORY" % va)
                continue
            s, n = serial_score(img, va, size)
            if s is None:
                print("  %08X  could not be scored" % va)
                continue
            rows.append((s, va, outcome, n, what))
            print("  %08X   %-16s %-7.2f %-6d %s" % (va, outcome, s, n, what))
        matched = [r[0] for r in rows if r[2] == "MATCHED"]
        stalled = [r[0] for r in rows if r[2] != "MATCHED"]
        print("")
        if matched and stalled:
            print("  matched: min %.2f, mean %.2f" % (min(matched),
                  sum(matched) / len(matched)))
            print("  stalled: max %.2f, mean %.2f" % (max(stalled),
                  sum(stalled) / len(stalled)))
            if min(matched) > max(stalled):
                print("\n  The metric SEPARATES them cleanly.")
            else:
                print("\n  The metric does NOT separate them cleanly, so treat")
                print("  the ranking as a hint and not as a filter.")
        return 0

    if args:
        for a in args:
            va = int(a, 16)
            size = inv.get(va)
            if size is None:
                print("%08X is not a known function start" % va)
                continue
            s, n = serial_score(img, va, size)
            print("%08X  %d B  %d instr  serial %s"
                  % (va, size, n, "%.2f" % s if s is not None else "unreadable"))
        return 0

    src = Path("build/candidates.txt")
    if not src.exists():
        print("%s missing -- run tools/candidates.py" % src)
        return 1
    rows = []
    for line in src.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        va, size = int(f[0], 16), int(f[1])
        s, n = serial_score(img, va, size)
        if s is not None:
            rows.append((s, n, va, size))
    rows.sort(key=lambda r: (-r[0], r[1]))

    print("%d of %d candidate(s) could be scored.\n" % (len(rows), len(rows)))
    print("Most serial first -- these have the least room for the compiler to")
    print("have chosen an ordering you cannot reach.\n")
    print("  %-10s %-7s %-7s %s" % ("address", "bytes", "instr", "serial"))
    for s, n, va, size in rows[:40]:
        print("  %08X   %-7d %-7d %.2f" % (va, size, n, s))
    out = Path("build/serial.txt")
    out.write_text("# address size instr serial\n" + "".join(
        "%08X %d %d %.3f\n" % (va, size, n, s) for s, n, va, size in rows))
    print("\nfull ranking -> %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
