"""Decode MSVC PowerPC switch dispatch, and say what the targets are.

    python tools/switches.py

TWO FORMS, both measured in this image rather than assumed. Neither is a
table of addresses, which is what Ghidra's PowerPCAddressAnalyzer looks for
and why it mishandles them -- there is no address anywhere, only an offset
and a base formed by a lis/addi pair.

BYTE FORM, 51 sites:

    cmplwi rV, N                   the bound: N+1 cases
    bgt-   default
    lis    r12, hi(byteTable)      byteTable is in .rdata
    addi   r12, r12, lo(byteTable)
    lbzx   r0, r12, rV             byte = byteTable[value]
    rlwinm r0, r0, 2, 0, 29        * 4, so the byte is a WORD index
    lis    r12, hi(caseBase)       caseBase is the word AFTER the bctr
    addi   r12, r12, lo(caseBase)
    add    r12, r12, r0
    mtctr  r12 ; bctr

HALFWORD FORM, 53 sites, for switches whose case bodies span more than 1 KB:

    cmplwi rV, N
    bgt-   default
    lis    r12, hi(halfTable)
    rlwinm r0, rV, 1, 0, 30        * 2 to index halfwords -- so the load's
    addi   r12, r12, lo(halfTable) index register is NOT the switch value
    lhzx   r0, r12, r0
    lis    r12, hi(caseBase)       no post-load scale: the halfword IS the
    addi   r12, r12, lo(caseBase)  byte offset
    add    r12, r12, r0
    mtctr  r12 ; bctr

The halfword form cost two bugs worth recording. The bound is a compare
against the SWITCH VALUE, and for this form the load's index register is the
value already scaled, so the search has to follow the `rlwinm` back -- keying
on the load's register found nothing at all, 104 of 104. And `rlwinm` is
M-form: its DESTINATION is rA and its source is rS, the opposite way round
from the D-form loads beside it. Reading it as a load left every one of the
53 halfword dispatches unbounded.

WHAT THIS ESTABLISHES. Case bodies are labels inside a function, not
functions, and nothing reaches them by `bl` or `b`, so `tools/discover.py`
cannot see them. The useful question is the reverse one -- whether anything
ALREADY listed as a function is really a case body -- and the answer over
2,571 recovered targets is none, including none from `.pdata`. That is the
check this tool exists to run.

It also does NOT explain the 13 functions Ghidra lists and discovery does
not: none of them is a case target.
"""

import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions, load_inventory
from discover import exec_ranges
import ppcdis

BCTR = 0x4E800420
BCTRL = 0x4E800421


def op(w):
    return (w >> 26) & 0x3F


def xo(w):
    return (w >> 1) & 0x3FF


def rt(w):
    return (w >> 21) & 0x1F


def ra(w):
    return (w >> 16) & 0x1F


def rb(w):
    return (w >> 11) & 0x1F


def simm(w):
    v = w & 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def find_dispatches(words, lo, ranges):
    """-> [(bctr_va, case_base, table_va, index_load)] for each dispatch found.

    Deliberately strict: the exact three-instruction tail
    `add rX,rX,rY ; mtctr rX ; bctr` has to be present, and the base has to
    come from a lis/addi pair into the same register. Anything looser starts
    matching virtual calls, of which this image has about 13,000.
    """
    out = []
    for i, w in enumerate(words):
        if w not in (BCTR, BCTRL):
            continue
        if i < 3:
            continue
        mt = words[i - 1]
        # mtctr is mtspr with SPR=9: encoded 467<<1 in X form.
        if op(mt) != 31 or xo(mt) != 467:
            continue
        ctr_src = rt(mt)
        ad = words[i - 2]
        if op(ad) != 31 or xo(ad) != 266:          # add
            continue
        if rt(ad) != ctr_src:
            continue
        base_reg = ra(ad)
        idx_reg = rb(ad)

        value_reg = None
        load_at = None
        # Walk back for `addi base_reg, X, lo` and `lis X, hi`.
        case_base = None
        for k in range(i - 3, max(-1, i - 16), -1):
            a = words[k]
            if op(a) == 14 and rt(a) == base_reg:          # addi
                src = ra(a)
                for j in range(k - 1, max(-1, k - 6), -1):
                    b = words[j]
                    if op(b) == 15 and rt(b) == src:       # lis (addis rX,0)
                        if ra(b) == 0:
                            case_base = ((simm(b) & 0xFFFF) << 16) + simm(a)
                            case_base &= 0xFFFFFFFF
                        break
                break

        # Walk back for the indexed byte/half/word load that produced idx.
        table_va, kind = None, None
        for k in range(i - 3, max(-1, i - 20), -1):
            a = words[k]
            if op(a) != 31:
                continue
            if xo(a) not in (87, 279, 23):                 # lbzx lhzx lwzx
                continue
            kind = {87: "lbzx", 279: "lhzx", 23: "lwzx"}[xo(a)]
            treg = ra(a)
            value_reg = rb(a)
            load_at = k
            # For the halfword form the index is the value ALREADY SCALED by
            # a preceding `rlwinm rX,rY,1,0,30`, so rb is not the switch
            # value and the bound search must follow it back to rY. That is
            # why all 53 lhzx dispatches came back unbounded.
            # rlwinm is M-form: the DESTINATION is rA and the source is rS,
            # the opposite way round from the D-form loads above. Reading it
            # as a load left value_reg pointing at the scaled register and
            # every one of the 53 halfword dispatches came back unbounded.
            for j in range(k - 1, max(-1, k - 8), -1):
                b2 = words[j]
                if op(b2) == 21 and ra(b2) == value_reg:    # rlwinm rA <- rS
                    value_reg = rt(b2)
                    break
            for j in range(k - 1, max(-1, k - 8), -1):
                b = words[j]
                if op(b) == 14 and rt(b) == treg:
                    src = ra(b)
                    for m in range(j - 1, max(-1, j - 6), -1):
                        c = words[m]
                        if op(c) == 15 and rt(c) == src and ra(c) == 0:
                            table_va = (((simm(c) & 0xFFFF) << 16)
                                        + simm(b)) & 0xFFFFFFFF
                            break
                    break
            break

        # THE BOUND. A switch guards its dispatch with `cmplwi rIdx, N` and
        # branches away when the value exceeds N, so the byte table has
        # exactly N+1 entries. Without it there is nothing to say where the
        # table ends, and reading a fixed 256 bytes pulls in unrelated data
        # and invents case targets -- which showed up as 7 collisions with
        # .pdata function starts, i.e. as targets that cannot be real.
        # The guard compares the SWITCH VALUE -- the index register of the
        # lbzx -- not the scaled quantity that reaches the `add`. Comparing
        # against the latter found nothing at all, 104 of 104.
        bound = None
        for k in range(i - 3, max(-1, i - 32), -1):
            a = words[k]
            if op(a) == 10 and ra(a) == value_reg:          # cmpli / cmplwi
                bound = a & 0xFFFF
                break
            if op(a) == 11 and ra(a) == value_reg:          # cmpi / cmpwi
                bound = simm(a)
                break

        # Is the loaded value scaled by 4 AFTER the load? The byte form does
        # `rlwinm r0,r0,2,0,29`, so a byte is a WORD index. The halfword form
        # does not, so a halfword is already a byte offset.
        scale = 1
        if load_at is not None:
            for j in range(load_at + 1, min(len(words), load_at + 6)):
                b3 = words[j]
                if op(b3) == 21 and ra(b3) == rt(b3):       # rlwinm rX,rX,sh
                    sh = (b3 >> 11) & 0x1F
                    if sh == 2:
                        scale = 4
                    break

        if case_base is None:
            continue
        out.append((lo + i * 4, case_base, table_va, kind, bound, scale))
    return out


def main(argv):
    img = Image()
    ranges = exec_ranges(img)
    lo, hi = ranges[0]
    data = img.read(lo, hi - lo)
    n = len(data) // 4
    words = struct.unpack(">%dI" % n, data[:n * 4])

    disp = find_dispatches(words, lo, ranges)
    total_bctr = sum(1 for w in words if w in (BCTR, BCTRL))

    print("MSVC switch dispatch in .text\n")
    print("  bctr / bctrl sites                    %6d" % total_bctr)
    print("  of those, a decoded switch dispatch   %6d  (%.1f%%)"
          % (len(disp), 100.0 * len(disp) / max(total_bctr, 1)))
    print("  the rest are virtual calls and other computed jumps")
    print("")

    kinds = Counter(k for _b, _c, _t, k, _n, _s in disp)
    print("  index load form: %s"
          % ", ".join("%s x%d" % kv for kv in kinds.most_common()))
    no_table = sum(1 for _b, _c, t, _k, _n, _s in disp if t is None)
    no_bound = sum(1 for _b, _c, _t, _k, n, _s in disp if n is None)
    print("  dispatches whose table address was not recovered: %d" % no_table)
    print("  dispatches whose case COUNT was not recovered:    %d" % no_bound)
    print("")

    # Case targets, bounded by the containing function so a runaway table
    # cannot invent addresses.
    pdata = dict(load_functions())
    starts = sorted(pdata)
    import bisect

    def containing(va):
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None
        s = starts[i]
        return s if va < s + pdata.get(s, 0) else None

    targets = set()
    bounded, unbounded = 0, 0
    for bctr_va, base, table_va, kind, bound, scale in disp:
        host = containing(bctr_va)
        if table_va is None or bound is None or kind == "lwzx":
            unbounded += 1
            continue
        if not (0 <= bound < 4096):
            unbounded += 1
            continue
        width = 1 if kind == "lbzx" else 2
        end = (host + pdata[host]) if host else (base + 4096)
        raw = img.read(table_va, (bound + 1) * width)
        if raw is None or len(raw) != (bound + 1) * width:
            unbounded += 1
            continue
        bounded += 1
        for e in range(bound + 1):
            if width == 1:
                v = raw[e]
            else:
                v = (raw[e * 2] << 8) | raw[e * 2 + 1]
            t = base + scale * v
            if base <= t < end and not (t & 3):
                targets.add(t)

    print("  case targets recovered (bounded by the containing function)")
    print("    from %d dispatch(es) with a byte table   %6d target(s)"
          % (bounded, len(targets)))
    print("    dispatches skipped, no usable table      %6d" % unbounded)
    print("")

    inv = dict(load_inventory())
    pset = set(starts)
    clash = sorted(set(inv) & targets)
    print("  DOES ANYTHING LISTED AS A FUNCTION SIT ON A CASE TARGET?")
    print("    inventory entries that are switch case bodies  %6d" % len(clash))
    print("    of those, from .pdata (would be a real defect)  %6d"
          % sum(1 for a in clash if a in pset))
    if clash:
        for a in clash[:10]:
            print("      %08X  %s" % (a, "from .pdata" if a in pset
                                      else "from discovery"))
    out = Path("build/switch_targets.txt")
    out.write_text("# address  (MSVC switch case bodies, not functions)\n"
                   + "".join("%08X\n" % a for a in sorted(targets)))
    print("")
    print("  -> %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
