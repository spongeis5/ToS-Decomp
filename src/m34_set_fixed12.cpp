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
// `fctidz` + `stfd` to the red zone + `lwz` of the LOW word is MSVC's whole
// float-to-int sequence on this target; the second arm converts from f1
// again, so it is `(int)v` and not a reuse of `v * 16`.
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
        int n = (int)(v * 16.0f);

        if (n & 15)
        {
            f->packed = (u16)((f->packed & 0xF000) | (n & 0x0FFF));
            f->flags |= 0x10;
            return;
        }
    }

    f->packed = (u16)((f->packed & 0xF000) | ((int)v & 0x0FFF));
    f->flags &= ~0x10;
}
