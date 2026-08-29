#include "types.h"

// sub_8253FD90 -- add up the elements of two int arrays selected by two bit
// masks, one bit per element, LSB first. 148 B, 9 callers.
//
//      li      r9,11 ; li r11,0 ; mr r10,r3 ; mtctr r9
//  L1: clrlwi  r9,r4,31          m1 & 1
//      cmplwi  cr6,r9,0 ; beq- skip1
//      lwz     r9,0(r10) ; add r11,r9,r11
// skip1:rlwinm r8,r4,31,1,31     m1 >> 1
//      clrlwi  r9,r8,31
//      cmplwi  cr6,r9,0 ; beq- skip2
//      lwz     r9,4(r10) ; add r11,r9,r11
// skip2:rlwinm r4,r8,31,1,31
//      addi    r10,r10,8
//      bdnz+   L1
//      li      r9,13 ; addi r10,r3,92 ; mtctr r9
//  L2: ... same body on -4(r10) and 0(r10) with m2 ...
//      mr      r3,r11
//      blr
//
// Two counted loops, each UNROLLED BY TWO by the compiler: ctr is 11 and 13,
// the bodies handle a pair of elements, so the trip counts are 22 and 26.
// The pointer walks by 8 with a pair of displacements, and the second loop is
// biased -- base r3+92 with loads at -4 and 0 -- so its array starts at
// offset 88, right after 22 words.
//
// `rlwinm ...,31,1,31` is a LOGICAL right shift by one, so both masks are
// unsigned; `clrlwi ...,31` is `& 1`.
//
// `add r11,r9,r11` puts the loaded element in rA and the running total in rB,
// which is the `+=` order: the later source read is the element.
struct MaskedTable
{
    /* 0x00 */ s32 a[22];
    /* 0x58 */ s32 b[26];
};
ASSERT_OFFSET(MaskedTable, b, 0x58);

s32 SumMasked(MaskedTable* t, u32 m1, u32 m2)
{
    s32 sum = 0;

    for (s32 i = 0; i < 22; i++)
    {
        if (m1 & 1)
            sum += t->a[i];
        m1 >>= 1;
    }

    for (s32 j = 0; j < 26; j++)
    {
        if (m2 & 1)
            sum += t->b[j];
        m2 >>= 1;
    }

    return sum;
}
