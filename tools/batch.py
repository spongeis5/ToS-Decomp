"""Dump the next N match candidates, ranked by caller count, with disassembly.

    python tools/batch.py                 40 candidates, most-called first
    python tools/batch.py 20              20 of them
    python tools/batch.py 20 --skip 40    the next 20 after those
    python tools/batch.py --max-bytes 64  only the small ones
    python tools/batch.py --no-vmx        skip anything using VMX128

This is step 2 of the loop the README describes, and it used to be an ad-hoc
script rewritten from memory each session. Ranking by CALLER COUNT rather
than by address is the whole point: a function with 40 callers is a shared
accessor whose shape recurs, so recognising it once pays repeatedly, while
the same effort spent on a single-caller function buys one match.

Excludes anything already in `src/manifest.txt` or `src/attempts.txt`, so
running it twice does not hand back work already done.

The disassembly carries the same `lis`/`addi` annotation `tools/disasm.py`
does -- the resolved address, and the string if it lands on one -- because a
global reference that reads as two opaque halves is the single most common
way a candidate looks unreadable when it is not.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from disasm import string_at

import ppcdis

CAND = Path("build/candidates.txt")
CALLS = Path("build/callgraph.txt")
MANIFEST = Path("src/manifest.txt")
ATTEMPTS = Path("src/attempts.txt")


def done_addresses():
    """Every address that already has a source, matched or not."""
    out = set()
    for p in (MANIFEST, ATTEMPTS):
        if not p.exists():
            continue
        for line in p.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    out.add(int(parts[1], 16))
                except ValueError:
                    pass
    return out


def caller_counts():
    counts = {}
    if not CALLS.exists():
        print("build/callgraph.txt is missing -- run tools/discover.py first.")
        print("Refusing to rank by a caller count of zero for everything,")
        print("because that silently degrades to ranking by address.")
        sys.exit(1)
    for line in CALLS.read_text().splitlines():
        if line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        try:
            callee = int(parts[1], 16)
        except ValueError:
            continue
        counts[callee] = counts.get(callee, 0) + 1
    return counts


def candidates():
    if not CAND.exists():
        print("build/candidates.txt is missing -- run tools/candidates.py.")
        sys.exit(1)
    rows = []
    for line in CAND.read_text().splitlines():
        if line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        rows.append({
            "va": int(parts[0], 16),
            "size": int(parts[1]),
            "floats": int(parts[2]),
            "vmx": int(parts[3]),
        })
    return rows


def dump(img, va, size, fstarts):
    pending = {}
    n = size // 4
    lines = {a: (w, t) for a, w, t in ppcdis.image_range(va, n)}
    for i in range(n):
        a = va + i * 4
        raw = img.read(a, 4)
        if raw is None:
            print("%08X  <not backed>" % a)
            continue
        w = struct.unpack(">I", raw)[0]
        note = ""
        op = w >> 26
        if op == 15 and ((w >> 16) & 0x1F) == 0:
            pending[(w >> 21) & 0x1F] = w & 0xFFFF
        elif op == 14:
            ra = (w >> 16) & 0x1F
            if ra in pending:
                lo = w & 0xFFFF
                if lo >= 0x8000:
                    lo -= 0x10000
                val = ((pending[ra] << 16) + lo) & 0xFFFFFFFF
                s = string_at(img, val)
                note = "   ; = %08X%s" % (val, ('  "%s"' % s) if s else "")
        elif op == 24:
            rs = (w >> 21) & 0x1F
            if rs in pending:
                val = ((pending[rs] << 16) | (w & 0xFFFF)) & 0xFFFFFFFF
                s = string_at(img, val)
                note = "   ; = %08X%s" % (val, ('  "%s"' % s) if s else "")
        elif op == 18:                                    # b / bl
            li = w & 0x03FFFFFC
            if li >= 0x02000000:
                li -= 0x04000000
            tgt = (li if (w & 2) else (a + li)) & 0xFFFFFFFF
            note = "   ; -> sub_%08X%s" % (
                tgt, "" if tgt in fstarts else "  (not a known start)")
        t = lines.get(a, (w, "<not disassembled>"))[1]
        print("%08X %08x  %-40s%s" % (a, w, t, note))


def main(argv):
    count = 40
    skip = 0
    max_bytes = None
    no_vmx = "--no-vmx" in argv
    for i, a in enumerate(argv[1:], 1):
        if a == "--skip":
            skip = int(argv[i + 1])
        elif a == "--max-bytes":
            max_bytes = int(argv[i + 1])
        elif not a.startswith("--") and a.isdigit():
            count = int(a)

    img = Image()
    inv = load_inventory()
    fstarts = {x for x, _ in inv}
    calls = caller_counts()
    done = done_addresses()

    rows = [r for r in candidates() if r["va"] not in done]
    if max_bytes is not None:
        rows = [r for r in rows if r["size"] <= max_bytes]
    if no_vmx:
        rows = [r for r in rows if r["vmx"] == 0]
    for r in rows:
        r["callers"] = calls.get(r["va"], 0)
    rows.sort(key=lambda r: (-r["callers"], r["size"], r["va"]))

    total = len(rows)
    rows = rows[skip:skip + count]
    print("; %d candidate(s) available after excluding %d already sourced;"
          % (total, len(done)))
    print("; showing %d, ranked by caller count, starting at %d.\n"
          % (len(rows), skip))

    for r in rows:
        print("=" * 68)
        print("sub_%08X   %d bytes   %d caller(s)   %d float op(s)%s"
              % (r["va"], r["size"], r["callers"], r["floats"],
                 "   %d VMX" % r["vmx"] if r["vmx"] else ""))
        print("=" * 68)
        dump(img, r["va"], r["size"], fstarts)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
