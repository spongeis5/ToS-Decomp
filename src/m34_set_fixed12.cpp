// sub_82761808 -- store a float into a 12-bit field of a halfword, and set or
// clear a "has a fraction" bit beside it. 116 bytes, 4 callers.
//
//      lis  r11,-32255 ; lfs f0,-21384(r11)     -> 8200AC78, which holds 256.0f
//      fcmpu cr6,f1,f0 ; bge- cr6,<plain>
//      lis  r11,-32256 ; lfs f0,12288(r11)      -> 82003000, which holds 16.0f
//      fmuls f0,f1,f0 ; fctidz f0,f0 ; stfd f0,-16(r1) ; lwz r11,-12(r1)
//      clrlwi. r10,r11,28 ; beq- <plain>
//      lhz  r10,4(r3) ; lhz r9,6(r3)
//      rlwimi r10,r11,0,20,31
//      ori  r11,r9,16
//      sth  r10,4(r3) ; sth r11,6(r3) ; blr
//  plain:
//      lhz  r11,4(r3) ; fctidz f0,f1 ; lhz r9,6(r3)
//      stfd f0,-16(r1) ; lwz r10,-12(r1)
//      rlwimi r11,r10,0,20,31
//      andi. r10,r9,65519
//      sth  r11,4(r3) ; sth r10,6(r3) ; blr
//
// `fctidz` + `stfd` to the red zone + `lwz` of the LOW word is the UNSIGNED
// conversion, and that one word decides the type. `fctiwz` is signed-only, so
// MSVC reaches for the 64-bit convert and keeps the low half whenever the
// destination is `unsigned`; writing `(int)` gives `fctiwz` and nothing else
// in the function changes -- 3 of 29, every register renamed downstream.
// The second arm converts from f1 again, so it is a fresh `(u32)v` and not a
// reuse of `v * 16`.
//
// `clrlwi.` doing the mask and the test in ONE instruction is the /Os form of
// that pair; at /O2 it splits into `clrlwi` plus a `cmpwi cr6` and the whole
// tail shifts by a word. Same one-word signature as sub_827156B8. The `ori`
// in the first arm agrees -- retail writes into r11, /O2 gives it a fresh r8.
//
// `rlwimi rD,rS,0,20,31` inserts rS's low twelve bits into rD's low twelve
// with no rotate, over a value that arrived by `lhz` -- so the field is the
// bottom 12 bits of the halfword at +4 and the top 4 are preserved. The
// output does NOT decide whether that was spelled as a bitfield or as an
// explicit mask, so it is written as the mask, which claims less.
//
// `andi.` is the only immediate AND PowerPC has, so its record bit is an
// artefact and not a test.
//
// Both constants were read out of the image rather than inferred: 256.0f is
// the range check and 16.0f the fixed-point scale, which agrees with the
// twelve bits (256 * 16 == 4096).
//
// 4 of 29 words are relocated.
//
// NEAR MISS: 17 of 25 non-relocated words at /O2 /Os, 120 bytes against 116.
// The FIRST arm is exact -- every word from 82761808 to 82761850 agrees --
// and the whole residue is in the second, where retail issues `lhz r9,6(r3)`
// between the `fctidz` and the `stfd` and we issue it after the `rlwimi`.
// That one slot decides the rest: with the flags load late, MSVC makes the
// CONVERTED VALUE the rlwimi destination (`rlwimi r10,r11,0,16,19`, inserting
// the old high nibble into the new value) instead of the packed word
// (`rlwimi r11,r10,0,20,31`), and then needs an `mr r11,r10` to get the
// result into the register the `sth` wants -- the four extra bytes.
//
// Ruled out, all identical at /O2 /Os:
//   * `f->flags &= ~0x10` versus `f->flags = (u16)(fl & 0xFFEF)` with `fl`
//     named ahead of the packed statement -- the load is folded straight back
//     and does not move, so the narrow-field naming lever does not reach it;
//   * declaring all three chains as locals in retail's own issue order
//     (packed, then the conversion, then flags), which is the direct
//     application of the sub_8216C240 declaration-order lever;
//   * plain /O2, which additionally splits `clrlwi.` and loses arm 1 as well
//     (3 of 25).
// The `(u32)` conversion and the level are both settled and neither is the
// remaining question; what is left is which operand MSVC picks as the rlwimi
// destination, and no spelling tried has moved it.

#include "types.h"

struct FixedPair
{
    /* 0x00 */ u8  unk0000[4];
    /* 0x04 */ u16 packed;
    /* 0x06 */ u16 flags;
};

ASSERT_OFFSET(FixedPair, packed, 4);
ASSERT_OFFSET(FixedPair, flags, 6);

void SetFixed(FixedPair* f, float v)
{
    if (v < 256.0f)
    {
        u32 n = (u32)(v * 16.0f);

        if (n & 15)
        {
            f->packed = (u16)((f->packed & 0xF000) | (n & 0x0FFF));
            f->flags |= 0x10;
            return;
        }
    }

    u16 p = f->packed;
    u32 m = (u32)v;
    u16 fl = f->flags;

    f->packed = (u16)((p & 0xF000) | (m & 0x0FFF));
    f->flags = (u16)(fl & 0xFFEF);
}
