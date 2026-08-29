"""Named translation units, from the assert-registration stubs.

    python tools/tunits.py              every named unit and its members
    python tools/tunits.py --targets    the best functions to match in them

`tools/srcfiles.py` finds ten places where the title's own source paths are
formed:

    c:\\branches\\SB09\\main\\NG\\Source\\Engine\\Graphics\\Builder.cpp
    c:\\branches\\SB09\\main\\NG\\Source\\Engine\\System\\Time.cpp
    ...

Each is a tiny stub that hands the path to a registrar along with one or two
GLOBALS -- the file's own static state, allocated in that translation unit:

    sub_82909D80:
        lis   r11,-32249 ; addi r5,r11,-19296   the PATH
        lis   r10,-32092 ; addi r4,r10,308      82A40134
        lis   r9,-32092  ; addi r3,r9,460       82A401CC
        b     0x825FE8B0

That is the lever nothing had used. The path names the FILE; the globals
belong to that file; and **any function that references one of those globals
is in that same translation unit**. `tools/segment.py` scores 55% precision
clustering by adjacency; this is not a guess at all.

What it buys, in the project's own terms: README gap 3 is "TU splitting was
attempted and the result is mostly NEGATIVE". This closes it for ten units by
name, which is enough to check the adjacency rule against something real.
"""

import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
WINDOW = 24


def own_paths():
    """The title's own source paths, with the address that forms each."""
    p = ROOT / "build/source_files.txt"
    if not p.exists():
        print("build/source_files.txt is missing -- run tools/srcfiles.py.")
        sys.exit(1)
    out = []
    for line in p.read_text(errors="replace").splitlines():
        f = line.split("\t")
        if len(f) < 3:
            continue
        low = f[0].lower()
        if "sb09" not in low or "\\ng\\" not in low:
            continue
        if not low.endswith(".cpp"):
            continue
        out.append((f[0], [int(x, 16) for x in f[2].split()]))
    return out


def formed_addresses(img, va, count=16):
    """Every address a short run of code builds with lis+addi/ori."""
    out = []
    pending = {}
    for i in range(count):
        raw = img.read(va + i * 4, 4)
        if raw is None:
            break
        w = struct.unpack(">I", raw)[0]
        op = w >> 26
        if op == 15 and ((w >> 16) & 0x1F) == 0:
            pending[(w >> 21) & 0x1F] = w & 0xFFFF
        elif op == 14:
            ra = (w >> 16) & 0x1F
            if ra in pending:
                lo = w & 0xFFFF
                if lo >= 0x8000:
                    lo -= 0x10000
                out.append(((pending[ra] << 16) + lo) & 0xFFFFFFFF)
        elif op == 24:
            rs = (w >> 21) & 0x1F
            if rs in pending:
                out.append(((pending[rs] << 16) | (w & 0xFFFF)) & 0xFFFFFFFF)
        if w in (0x4E800020, 0x4E800420) or ((w >> 26) == 18 and not (w & 1)):
            break
    return out


def scan_references(img, wanted):
    """-> {global address: [function addresses that form it]}"""
    inv = sorted(load_inventory())
    starts = [a for a, _ in inv]
    sizes = dict(inv)

    def owner(a):
        lo, hi, best = 0, len(starts) - 1, None
        while lo <= hi:
            m = (lo + hi) // 2
            if starts[m] <= a:
                best = starts[m]
                lo = m + 1
            else:
                hi = m - 1
        if best is not None and best <= a < best + sizes[best]:
            return best
        return None

    hits = defaultdict(set)
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        off = s["va"] - img.base
        avail = len(img.data) - off
        size = min(s["vsize"] or s["rawsz"], s["rawsz"], max(avail, 0))
        n = size // 4
        if n <= 0:
            continue
        words = struct.unpack_from(">%dI" % n, img.data, off)
        for i, w in enumerate(words):
            if (w >> 26) != 15 or ((w >> 16) & 0x1F) != 0:
                continue
            rD, hi16 = (w >> 21) & 0x1F, w & 0xFFFF
            for j in range(i + 1, min(i + 1 + WINDOW, n)):
                w2 = words[j]
                op = w2 >> 26
                val = None
                if op == 14 and ((w2 >> 16) & 0x1F) == rD:
                    lo = w2 & 0xFFFF
                    if lo >= 0x8000:
                        lo -= 0x10000
                    val = ((hi16 << 16) + lo) & 0xFFFFFFFF
                elif op == 24 and ((w2 >> 21) & 0x1F) == rD:
                    val = ((hi16 << 16) | (w2 & 0xFFFF)) & 0xFFFFFFFF
                if val is not None:
                    if val in wanted:
                        o = owner(s["va"] + i * 4)
                        if o is not None:
                            hits[val].add(o)
                    break
                if ((w2 >> 21) & 0x1F) == rD and op in (14, 15, 24, 31):
                    break
    return hits


def main(argv):
    img = Image()
    inv = dict(load_inventory())

    done = set()
    for fn in ("src/manifest.txt", "src/attempts.txt"):
        p = ROOT / fn
        if p.exists():
            for line in p.read_text().splitlines():
                line = line.split("#")[0].strip()
                if line and len(line.split()) >= 2:
                    try:
                        done.add(int(line.split()[1], 16))
                    except ValueError:
                        pass

    units = []
    for path, sites in own_paths():
        for site in sites:
            formed = formed_addresses(img, site)
            # the first formed address is the path string itself
            globs = [a for a in formed[1:] if a not in (site,)]
            units.append((path, site, globs))

    wanted = set()
    for _p, _s, gl in units:
        wanted.update(gl)
    print("%d named translation unit stub(s); %d distinct global(s) between"
          % (len(units), len(wanted)))
    print("them. Scanning every lis/addi pair in .text for references...\n")
    hits = scan_references(img, wanted)

    rows = []
    for path, site, globs in units:
        members = set()
        for g in globs:
            members |= hits.get(g, set())
        members.discard(site)
        name = path.rsplit("\\", 1)[-1]
        rows.append((name, path, site, globs, sorted(members)))

    rows.sort(key=lambda r: -len(r[4]))
    for name, path, site, globs, members in rows:
        print("%s" % path)
        print("  stub %08X   globals %s"
              % (site, " ".join("%08X" % g for g in globs)))
        print("  %d function(s) reference them:" % len(members))
        for m in members:
            mark = "  [done]" if m in done else ""
            print("     %08X  %5d B%s" % (m, inv.get(m, 0), mark))
        print("")

    if "--targets" in argv:
        print("=" * 62)
        print("BEST TARGETS: smallest unmatched member of each named unit")
        print("=" * 62)
        for name, path, site, globs, members in rows:
            todo = [(inv.get(m, 0), m) for m in members if m not in done]
            todo = [t for t in todo if t[0]]
            todo.sort()
            if todo:
                print("  %-22s %08X  %4d B" % (name, todo[0][1], todo[0][0]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
