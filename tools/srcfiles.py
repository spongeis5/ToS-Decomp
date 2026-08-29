"""Harvest every source-file path the image forms an address to.

The title registers translation units at static-init time: each initializer
builds the address of its own `__FILE__` string and stores it into a global
list.  So the set of source paths REFERENCED BY CODE is recoverable, and it
is a far better map of the project than the handful of strings that happen to
sit near each other in .rdata.

Scans every `lis`+`addi`/`ori` pair in executable sections, resolves the
address, and keeps the ones landing on a printable string that looks like a
source path.  Reports the reference site so each can be read in context.
"""

import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions

WINDOW = 24


def looks_like_source(s):
    low = s.lower()
    if any(low.endswith(e) for e in (".cpp", ".c", ".h", ".hpp", ".inl", ".cxx")):
        return True
    return False


def string_at(img, va, limit=140):
    b = img.read(va, limit)
    if not b:
        return None
    out = []
    for c in b:
        if c == 0:
            break
        if not (0x20 <= c < 0x7F):
            return None
        out.append(chr(c))
    if len(out) < 5:
        return None
    return "".join(out)


def main():
    img = Image()
    funcs = load_functions()
    starts = sorted(a for a, _ in funcs)
    sizes = dict(funcs)

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

    hits = defaultdict(list)
    words_scanned = 0
    cache = {}

    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        base_off = s["va"] - img.base
        size = s["vsize"] or s["rawsz"]
        n = size // 4
        words = struct.unpack_from(">%dI" % n, img.data, base_off)
        words_scanned += n
        for i, w in enumerate(words):
            if (w >> 26) != 15 or ((w >> 16) & 0x1F) != 0:
                continue
            rD = (w >> 21) & 0x1F
            hi = w & 0xFFFF
            for j in range(i + 1, min(i + 1 + WINDOW, n)):
                w2 = words[j]
                op = w2 >> 26
                val = None
                if op == 14 and ((w2 >> 16) & 0x1F) == rD:
                    lo = w2 & 0xFFFF
                    if lo >= 0x8000:
                        lo -= 0x10000
                    val = ((hi << 16) + lo) & 0xFFFFFFFF
                elif op == 24 and ((w2 >> 21) & 0x1F) == rD:
                    val = ((hi << 16) | (w2 & 0xFFFF)) & 0xFFFFFFFF
                if val is not None:
                    if val not in cache:
                        st = string_at(img, val)
                        cache[val] = st if (st and looks_like_source(st)) else None
                    if cache[val]:
                        hits[cache[val]].append(s["va"] + i * 4)
                    break
                if ((w2 >> 21) & 0x1F) == rD and op in (14, 15, 24, 32, 36):
                    break

    print("scanned %d instruction word(s)" % words_scanned)
    print("%d distinct source path(s) referenced by code\n" % len(hits))

    own = sorted(k for k in hits if "branches" in k.lower() or "sb09" in k.lower())
    other = sorted(k for k in hits if k not in own)

    print("=== THE TITLE'S OWN SOURCE (%d) ===" % len(own))
    for k in own:
        print("  %-96s  %d ref(s)  first at %08X"
              % (k, len(hits[k]), hits[k][0]))
    print("\n=== everything else (%d) ===" % len(other))
    for k in other[:40]:
        print("  %-96s  %d ref(s)" % (k[:96], len(hits[k])))
    if len(other) > 40:
        print("  ... %d more" % (len(other) - 40))

    out = Path("build/source_files.txt")
    with out.open("w") as f:
        for k in sorted(hits):
            f.write("%s\t%d\t%s\n"
                    % (k, len(hits[k]), " ".join("%08X" % a for a in hits[k][:12])))
    print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
