#include "types.h"

// sub_82831A48 -- widest of three integer ranges, turned into a bit count and
// clamped to 24. 196 B, 6 callers.
//
//   lwz r10,12(r3) ; lwz r11,16(r3) ; lwz r9,24(r3) ; lwz r8,20(r3)
//   subf r11,r10,r11        hi0 - lo0
//   subf r10,r8,r9          hi1 - lo1
//   cmpw cr6,r11,r10 ; bgt- +8 ; mr r11,r10          <- max
//   lwz r10,32(r3) ; lwz r9,28(r3) ; subf r10,r9,r10
//   cmpw cr6,r11,r10 ; bgt- +8 ; mr r11,r10          <- max
//   mr r9,r11 ; li r10,0 ; cmplwi cr6,r11,0 ; beq- out
//   L: rlwinm r9,r9,31,1,31 ; addi r10,r10,1 ; cmplwi cr6,r9,0 ; bne+ L
//   addi r9,r10,-4 ; li r8,1 ; li r10,3 ; slw r9,r8,r9
//   li r8,-1 ; mtctr r10 ; add r9,r9,r11
//   ... the SAME bit-length loop again, three times under bdnz ...
//   cmpwi cr6,r8,24 ; ble- +16 ; li r11,24 ; stw r11,8(r3) ; blr
//   stw r8,8(r3) ; blr
//
// Three facts read off the listing rather than guessed:
//
// * The pairs are (12,16), (20,24), (28,32) -- stride 8 -- so the object
//   holds three {lo,hi} ranges at 0x0C, not two separate vectors.
// * `bgt-` SKIPS the `mr`, so the assignment happens on `<=`: this is a MAX,
//   not a min. The accumulator is the first operand of every `cmpw`.
// * `rlwinm rX,rX,31,1,31` is a LOGICAL right shift, and the loop test is
//   `cmplwi`, so the value being shifted down is unsigned. The `cmpw` on the
//   extents and on the running maximum are signed.
//
// The three-iteration `bdnz` loop is genuinely loop-INVARIANT: nothing in the
// body writes r9. `li r10,3 ; mtctr` with the induction variable never read
// says the counter is only a counter, so the source really does recompute the
// same bit length three times and take the maximum. Written literally.
//
// The clamp is an if/else with TWO stores rather than one, because the `> 24`
// arm is the fall-through of `ble-` -- so it is written first.

struct Range3
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ s32  bits;
    /* 0x0C */ s32  lo0;
    /* 0x10 */ s32  hi0;
    /* 0x14 */ s32  lo1;
    /* 0x18 */ s32  hi1;
    /* 0x1C */ s32  lo2;
    /* 0x20 */ s32  hi2;
};
ASSERT_OFFSET(Range3, bits, 0x08);
ASSERT_OFFSET(Range3, lo0,  0x0C);
ASSERT_OFFSET(Range3, hi2,  0x20);

void ComputeExtentBits(Range3* r)
{
    s32 w = r->hi0 - r->lo0;
    s32 t = r->hi1 - r->lo1;
    if (w <= t)
        w = t;
    t = r->hi2 - r->lo2;
    if (w <= t)
        w = t;

    u32 v = (u32)w;
    s32 n = 0;
    while (v != 0)
    {
        v >>= 1;
        n++;
    }

    s32 best = -1;
    u32 q = (u32)w + (1u << (n - 4));

    for (s32 i = 0; i < 3; i++)
    {
        u32 x = q;
        s32 m = 0;
        while (x != 0)
        {
            x >>= 1;
            m++;
        }
        if (m > best)
            best = m;
    }

    if (best > 24)
        r->bits = 24;
    else
        r->bits = best;
}
