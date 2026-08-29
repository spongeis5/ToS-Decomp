"""Prove match.can_shrink accepts a genuine over-long row and nothing else.

    python tools/test_shrink.py

`match.py` may cut its comparison window down to the length of OUR code, on
the grounds that the recorded size covers more than one function -- one
`.pdata` unwind row can span a run of adjacent frameless bodies, and
8215E5B0 is recorded as 156 bytes while holding six of them.

That is a relaxation of the size check, so it is exactly the kind of change
that can turn a half-decompiled function into a reported MATCH. Six cases:
one that must be accepted and five that must not. A relaxation with only a
positive test is not tested.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from match import can_shrink, can_extend, _is_real_start

BASE = 0x82000000
BLR = 0x4E800020
BEQLR = 0x4D9A0020                 # conditional: beqlr cr6
LWZ = 0x81630008                   # lwz r11,8(r3)
LIS = 0x3D608207                   # lis r11,-32249
B_FWD = 0x48000010                 # b +0x10 -- lands past a 8-byte body
B_TAIL = 0x4BFFFF5C                # b <somewhere back>, a tail call


def words(*ws):
    return struct.pack(">%dI" % len(ws), *ws)


def clean(n):
    return b"\x01" * n


def masked(n, reloc_word):
    """A mask with one word marked relocated, as coff_functions produces."""
    m = bytearray(b"\x01" * n)
    m[reloc_word * 4:reloc_word * 4 + 4] = b"\x00\x00\x00\x00"
    return bytes(m)


def case(label, expect, code, mask, tbytes, tsize):
    got = can_shrink(code, mask, tbytes, BASE, tsize)
    ok = (got == expect)
    print("  %-4s %-56s %s"
          % ("PASS" if ok else "FAIL", label,
             "shrinks" if got else "refuses"))
    return ok


def main():
    print("match.can_shrink -- two cases must be accepted, five refused\n")
    print("  %-4s %-56s %s" % ("", "case", "verdict"))
    r = []

    # The real shape: our two-word body ends in blr, the row holds a second
    # body after it, and nothing in ours reaches that far.
    ours = words(LWZ, BLR)
    theirs = words(LWZ, BLR, LIS, BLR)
    r.append(case("two-word body, second body follows (must shrink)",
                  True, ours, clean(8), theirs, 16))

    # The tail-call form, where the last word is relocated and therefore
    # differs. This is the case a raw byte comparison would silently reject,
    # making the whole relaxation dead code that still reads as working.
    ours_t = words(LWZ, B_TAIL)
    theirs_t = words(LWZ, 0x4BFFFE78, LIS, BLR)     # different displacement
    r.append(case("tail call, displacement relocated (must shrink)",
                  True, ours_t, masked(8, 1), theirs_t, 16))

    # (1) our last word is a CONDITIONAL return: control can fall through it
    # into the bytes we are proposing to discard.
    ours_c = words(LWZ, BEQLR)
    theirs_c = words(LWZ, BEQLR, LIS, BLR)
    r.append(case("ends in a conditional return (must refuse)",
                  False, ours_c, clean(8), theirs_c, 16))

    # (2) a branch inside our code jumps INTO the leftover range.
    ours_e = words(B_FWD, LWZ, LWZ, BLR)            # b +0x10 = past our 16 B
    theirs_e = words(B_FWD, LWZ, LWZ, BLR, LIS, BLR)
    r.append(case("a branch escapes into the leftover range (must refuse)",
                  False, ours_e, clean(16), theirs_e, 24))

    # (3) the retail word in the last position is NOT a terminator, so the
    # retail function does not end where ours does.
    theirs_n = words(LWZ, LWZ, LIS, BLR)
    r.append(case("retail word there is not a terminator (must refuse)",
                  False, ours, clean(8), theirs_n, 16))

    # (4) a non-relocated word of the prefix disagrees.
    theirs_d = words(LIS, BLR, LIS, BLR)
    r.append(case("a non-relocated prefix word differs (must refuse)",
                  False, ours, clean(8), theirs_d, 16))

    # (5) -- the hole that clause (4) leaves open on its own. A one-word
    # body whose only word is relocated has NO non-relocated word to agree,
    # so (4) is vacuously true and the shrink would report a match having
    # verified nothing. This is the case that was found in the wild.
    ours_1 = words(B_TAIL)
    theirs_1 = words(0x4BFFFE78, LIS, BLR)
    r.append(case("only word is relocated, nothing verified (must refuse)",
                  False, ours_1, masked(4, 0), theirs_1, 12))

    # ---- can_extend, the mirror relaxation -------------------------------
    # It grows the window when the recorded size is too short. Same danger in
    # the other direction: without the bound and the tail check it would let
    # a source that produces a function AND ITS NEIGHBOUR pass.
    print("")
    print("match.can_extend -- two accepted, three refused")
    print("")
    print("  %-4s %-56s %s" % ("", "case", "verdict"))

    class FakeImg(object):
        def __init__(self, base, data):
            self.base, self.data = base, data

        def read(self, va, n):
            o = va - self.base
            if o < 0 or o + n > len(self.data):
                return None
            return self.data[o:o + n]

    BASE2 = 0x82000000
    # image: [LWZ BLR] then padding then [LIS BLR]
    image = words(LWZ, BLR, 0, LIS, BLR)
    img = FakeImg(BASE2 - 4, words(BLR) + image)   # a terminator before BASE2

    def ecase(label, expect, code, mask, tsize, sizes):
        got = can_extend(img, sizes, code, mask, BASE2, tsize) is not None
        ok = (got == expect)
        print("  %-4s %-56s %s"
              % ("PASS" if ok else "FAIL", label,
                 "extends" if got else "refuses"))
        return ok

    ours2 = words(LWZ, BLR)
    # the next start is fall-through reachable (preceded by LWZ), so it must
    # NOT bound us -- this is the sub_8262F658 case
    r.append(ecase("false start does not bound the extension (must extend)",
                   True, ours2, clean(8), 4, {BASE2: 4, BASE2 + 4: 4}))
    # A REAL next start CLOSER than our code length must bound it. It has to
    # be closer: a start at exactly len(code) is where our function ends, and
    # bounding there is correct rather than a refusal. The first version of
    # this case put it at +8 with 8 bytes of code and "failed" for that
    # reason -- the test was wrong, not the code.
    img2 = FakeImg(BASE2 - 4, words(BLR, BLR, LWZ, BLR))
    got = can_extend(img2, {BASE2: 4, BASE2 + 4: 4}, ours2, clean(8),
                     BASE2, 4) is not None
    ok = (got is False)
    print("  %-4s %-56s %s"
          % ("PASS" if ok else "FAIL",
             "a real next start INSIDE our code bounds it (must refuse)",
             "extends" if got else "refuses"))
    r.append(ok)
    # the extra word disagrees
    bad2 = words(LWZ, LIS)
    r.append(ecase("the extra word disagrees (must refuse)",
                   False, bad2, clean(8), 4, {BASE2: 4}))
    # the extra word disagrees but is RELOCATED, so it is excused
    r.append(ecase("the extra word is relocated, so excused (must extend)",
                   True, bad2, masked(8, 1), 4, {BASE2: 4}))
    # nothing to extend
    r.append(ecase("our code is not longer (must refuse)",
                   False, ours2, clean(8), 8, {BASE2: 8}))

    print("")
    print("%d of %d case(s) behaved as required." % (sum(r), len(r)))
    return 0 if all(r) else 1


if __name__ == "__main__":
    sys.exit(main())
