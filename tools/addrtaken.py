"""Function starts whose address is TAKEN IN CODE.

    python tools/addrtaken.py          -> build/addrtaken.txt

`discover.py` has two sources and neither can see this one:

  * the branch sweep reads `bl`/`b`, so it finds anything CALLED;
  * the data-pointer scan reads words in data sections, so it finds anything
    a vtable or table points at.

A function pointer formed in code and stored through a register is invisible
to both:

    lis  r11, hi
    addi r11, r11, lo        ; = 8215E5D0
    stw  r11, 32(r3)

That is how the six thunks packed into 8215E5B0..8215E650 are referenced,
which is why the inventory records that range as ONE 156-byte function --
and why `8215E5B0`, listed in MATCHED.md as a stall, was being compared
against 156 bytes when the function is 28.

TWO CONTAMINANTS, both removed, because each would otherwise inflate the
result with something that is not a function:

  * **MSVC switch case bodies.** A switch dispatch builds `caseBase` with a
    `lis`/`addi` pair, so every one of the 2,571 case targets `switches.py`
    recovered is an address formed in code. They are not functions. Before
    this exclusion the very first "new function" this reported was 82107B58,
    which is row one of build/switch_targets.txt.
  * **Addresses that are already known starts** -- kept, but only as the
    calibration set below.

The independent check is that the word before a candidate is an
unconditional terminator. That is not proof on its own, so it is CALIBRATED:
it is run first over the addresses that are already known function starts,
where the true answer is known, and the rate there is what any rate on the
unknowns has to be read against.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

import ppcdis

WINDOW = 32            # xref.py uses 16 and 26,825 sites hit that bound
BLR = 0x4E800020
BCTR = 0x4E800420
OUT = Path("build/addrtaken.txt")


def load_switch_targets():
    p = Path("build/switch_targets.txt")
    if not p.exists():
        print("build/switch_targets.txt is missing -- run tools/switches.py.")
        print("Refusing to continue: switch case bodies are formed with the")
        print("same lis/addi pair this scan looks for, so without that list")
        print("the result is inflated by an unknown amount rather than by a")
        print("stated one.")
        sys.exit(1)
    out = set()
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        out.add(int(line.strip(), 16))
    return out


def main(argv):
    img = Image()
    inv = load_inventory()
    starts = {a for a, _ in inv}
    byva = sorted(inv)
    addrs = [a for a, _ in byva]
    sizes = dict(byva)
    switch = load_switch_targets()

    def owner(a):
        lo, hi, best = 0, len(addrs) - 1, None
        while lo <= hi:
            m = (lo + hi) // 2
            if addrs[m] <= a:
                best = addrs[m]
                lo = m + 1
            else:
                hi = m - 1
        if best is not None and best <= a < best + sizes[best]:
            return best
        return None

    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = text["va"] + (text["vsize"] or text["rawsz"])

    found = {}
    sites = bound_hit = scanned = 0
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
        scanned += n
        for i, w in enumerate(words):
            if (w >> 26) != 15 or ((w >> 16) & 0x1F) != 0:
                continue
            sites += 1
            rD = (w >> 21) & 0x1F
            hi = w & 0xFFFF
            paired = False
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
                    paired = True
                    if tlo <= val < thi and (val & 3) == 0:
                        found.setdefault(val, []).append(s["va"] + i * 4)
                    break
                if ((w2 >> 21) & 0x1F) == rD and op in (14, 15, 24, 31):
                    paired = True
                    break
            if not paired:
                bound_hit += 1

    print("scanned %d instruction word(s) in the executable sections" % scanned)
    print("  %d lis site(s); %d ran to the %d-word lookahead bound without"
          % (sites, bound_hit, WINDOW))
    print("  pairing or being clobbered. That is a BOUND on this scan, not a")
    print("  statement about the image -- raise WINDOW to shrink it.")
    print()
    print("distinct .text addresses formed in code           %d" % len(found))

    contaminated = sorted(a for a in found if a in switch)
    print("  of those, MSVC switch case bodies (excluded)   %d" % len(contaminated))
    found = {a: v for a, v in found.items() if a not in switch}
    print("  remaining                                       %d" % len(found))

    known = [a for a in found if a in starts]
    interior = [a for a in found if a not in starts and owner(a) is not None]
    outside = [a for a in found if a not in starts and owner(a) is None]
    print("    already a known function start                %d (%.1f%%)"
          % (len(known), 100.0 * len(known) / len(found)))
    print("    inside a known function but not its start     %d" % len(interior))
    print("    in no known function at all                   %d" % len(outside))

    def terminator_before(a):
        raw = img.read(a - 4, 4)
        if raw is None:
            return False
        w = struct.unpack(">I", raw)[0]
        if w == BLR or w == BCTR or ((w >> 26) == 18 and not (w & 1)):
            return True
        if w == 0:
            raw2 = img.read(a - 8, 4)
            if raw2:
                w2 = struct.unpack(">I", raw2)[0]
                return (w2 == BLR or w2 == BCTR
                        or ((w2 >> 26) == 18 and not (w2 & 1)))
        return False

    def decodes(a):
        raw = img.read(a, 4)
        if raw is None:
            return False
        t = ppcdis.words([struct.unpack(">I", raw)[0]], a)[0][2].strip()
        return not (t.startswith(".long") or "invalid" in t.lower())

    print()
    print("CALIBRATION of the independent check")
    print("  The check is: the word before the address is an unconditional")
    print("  terminator. Run first on the addresses that are ALREADY known")
    print("  starts, where the answer is known, so the rate on the unknowns")
    print("  has something to be read against.")
    for label, group in (("known starts (calibration)", known),
                         ("interior, unknown", interior),
                         ("outside any function", outside)):
        if not group:
            continue
        ok = sum(1 for a in group if terminator_before(a))
        dec = sum(1 for a in group if decodes(a))
        print("    %-28s %5d   terminator before %5d (%5.1f%%)   decodes %d"
              % (label, len(group), ok, 100.0 * ok / len(group), dec))

    new = sorted(a for a in (interior + outside)
                 if terminator_before(a) and decodes(a))
    over = [a for a in new if owner(a) is not None]
    print()
    print("NEW function starts this source contributes: %d" % len(new))
    print("  interior to an existing entry, so that entry's recorded size is")
    print("  too long: %d" % len(over))

    cands = set()
    p = Path("build/candidates.txt")
    if p.exists():
        for line in p.read_text().splitlines():
            if not line.startswith("#") and line.split():
                cands.add(int(line.split()[0], 16))
    hurt = sorted({owner(a) for a in over if owner(a) in cands})
    print("  match candidates whose recorded size is therefore wrong: %d of %d"
          % (len(hurt), len(cands)))

    with OUT.open("w") as f:
        f.write("# function starts found by address-taken-in-code\n")
        f.write("# switch case bodies excluded; terminator-before check applied\n")
        f.write("# address  refs  interior_to\n")
        for a in new:
            o = owner(a)
            f.write("%08X %4d %s\n"
                    % (a, len(found[a]), ("%08X" % o) if o else "-"))
    print()
    print("wrote %s (%d row(s))" % (OUT, len(new)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
