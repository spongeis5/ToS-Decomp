"""Harvest the title's own profiler scope names.

The engine is instrumented: around a timebase read it pushes a literal name
and a timestamp onto a per-thread buffer.  Read out of sub_826731D8:

    lis  r9, 0x8207
    addi r8, r9, -0x452c      ; r8 = 8206BAD4 -> "TtrcSphere"
    stw  r8, 0(r10)           ; buffer[0] = name
    mftb r7                   ; timestamp
    stw  r7, 4(r10)           ; buffer[4] = timestamp

A first version of this tool searched loosely backwards from the `mftb` for
any `lis`/`addi` pair and kept whatever printable string it resolved to.  That
produced names like `144 Et` -- it was latching onto an unrelated pair and
reading an arbitrary address that happened to be printable.  A plausible
string is not evidence; the pattern has to be pinned.

So the chain is now required in full, and every link is checked:

  1. `mftb rT`
  2. after it, `stw rT, D(rBuf)`      -- the SAME register the mftb wrote
  3. before it, `stw rN, (D-4)(rBuf)` -- the SAME buffer register
  4. before that, the `lis`/`addi`|`ori` pair whose DESTINATION is rN

Any site not matching the whole chain is counted as unmatched rather than
guessed at, and the two counts are printed with their denominator.
"""

import bisect
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions

FWD = 8          # instructions after the mftb to find the timestamp store
BACK = 12        # instructions before it to find the name store
ADDR_BACK = 12   # instructions before that to find the lis/addi pair

# One known-good answer, read by hand out of the disassembly. The tool refuses
# to report if it cannot reproduce it.
KNOWN = (0x826731D8, "TtrcSphere")


def string_at(img, va, limit=64):
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
    s = "".join(out)
    return s if 2 <= len(s) <= 48 else None


def main():
    img = Image()
    funcs = load_functions()
    starts = [a for a, _ in funcs]
    sizes = dict(funcs)

    def owner(a):
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and starts[i] <= a < starts[i] + sizes[starts[i]]:
            return starts[i]
        return None

    names = defaultdict(Counter)
    name_sites = Counter()
    st = Counter()
    cache = {}

    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        off = s["va"] - img.base
        n = (s["vsize"] or s["rawsz"]) // 4
        words = struct.unpack_from(">%dI" % n, img.data, off)

        for i, w in enumerate(words):
            if (w >> 26) != 31 or ((w >> 1) & 0x3FF) != 371:
                continue
            st["sites"] += 1
            rT = (w >> 21) & 0x1F

            # 2. stw rT, D(rBuf)
            buf = None
            D = None
            for j in range(i + 1, min(i + 1 + FWD, n)):
                w2 = words[j]
                if (w2 >> 26) == 36 and ((w2 >> 21) & 0x1F) == rT:
                    buf = (w2 >> 16) & 0x1F
                    D = w2 & 0xFFFF
                    if D >= 0x8000:
                        D -= 0x10000
                    break
                if ((w2 >> 21) & 0x1F) == rT and (w2 >> 26) in (14, 15, 24, 31):
                    break                      # rT clobbered before its store
            if buf is None:
                st["no_timestamp_store"] += 1
                continue

            # 3. stw rN, (D-4)(rBuf)
            want = D - 4
            rN = None
            for j in range(i - 1, max(i - 1 - BACK, -1), -1):
                w2 = words[j]
                if (w2 >> 26) != 36:
                    continue
                if ((w2 >> 16) & 0x1F) != buf:
                    continue
                d2 = w2 & 0xFFFF
                if d2 >= 0x8000:
                    d2 -= 0x10000
                if d2 == want:
                    rN = (w2 >> 21) & 0x1F
                    idx = j
                    break
            if rN is None:
                st["no_name_store"] += 1
                continue

            # 4. the lis/addi|ori pair whose DESTINATION is rN
            val = None
            for k in range(idx - 1, max(idx - 1 - ADDR_BACK, -1), -1):
                w3 = words[k]
                op = w3 >> 26
                if op == 14 and ((w3 >> 21) & 0x1F) == rN:          # addi rN, rA, lo
                    rA = (w3 >> 16) & 0x1F
                    lo = w3 & 0xFFFF
                    if lo >= 0x8000:
                        lo -= 0x10000
                    for m in range(k - 1, max(k - 1 - ADDR_BACK, -1), -1):
                        w4 = words[m]
                        if (w4 >> 26) == 15 and ((w4 >> 16) & 0x1F) == 0 \
                           and ((w4 >> 21) & 0x1F) == rA:
                            val = (((w4 & 0xFFFF) << 16) + lo) & 0xFFFFFFFF
                            break
                    break
                if op == 24 and ((w3 >> 16) & 0x1F) == rN:          # ori rN, rS, lo
                    rS = (w3 >> 21) & 0x1F
                    for m in range(k - 1, max(k - 1 - ADDR_BACK, -1), -1):
                        w4 = words[m]
                        if (w4 >> 26) == 15 and ((w4 >> 16) & 0x1F) == 0 \
                           and ((w4 >> 21) & 0x1F) == rS:
                            val = (((w4 & 0xFFFF) << 16) | (w3 & 0xFFFF)) & 0xFFFFFFFF
                            break
                    break
            if val is None:
                st["no_address_pair"] += 1
                continue

            if val not in cache:
                cache[val] = string_at(img, val)
            nm = cache[val]
            if not nm:
                st["address_is_not_a_string"] += 1
                continue

            st["named"] += 1
            name_sites[nm] += 1
            o = owner(s["va"] + i * 4)
            if o is not None:
                names[o][nm] += 1
            else:
                st["outside_any_function"] += 1

    tot = st["sites"]
    print("%d mftb site(s)" % tot)
    for k in ("named", "no_timestamp_store", "no_name_store",
              "no_address_pair", "address_is_not_a_string"):
        print("   %-26s %5d  (%4.1f%%)" % (k, st[k], 100.0 * st[k] / max(tot, 1)))
    print("   %-26s %5d" % ("outside any function", st["outside_any_function"]))
    print()
    print("%d distinct name(s) over %d function(s)" % (len(name_sites), len(names)))

    # --- the known-good answer, or refuse ---
    got = names.get(KNOWN[0])
    ok = got is not None and KNOWN[1] in got
    print("\nknown-good check: sub_%08X -> %r : %s"
          % (KNOWN[0], KNOWN[1], "PASS" if ok else "FAIL"))
    if not ok:
        print("the extraction does not reproduce a name read by hand from the "
              "disassembly; refusing to report.")
        return 2

    game = sorted(n for n in name_sites if n.startswith("Ttz"))
    print("\n=== names with the game's own 'Ttz' prefix (%d) ===" % len(game))
    for n in game:
        print("   %-40s %d site(s)" % (n, name_sites[n]))

    out = Path("build/profiler_names.txt")
    with out.open("w") as f:
        f.write("# address size names\n")
        for o in sorted(names):
            f.write("%08X %6d %s\n"
                    % (o, sizes[o], " | ".join(sorted(names[o]))))
    print("\nwrote %s (%d function(s))" % (out, len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
