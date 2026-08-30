#include "types.h"

// sub_821032D8, sub_82103318, sub_82103358 -- three 56-byte state setters
// that record a DWORD and fold it into a 4-bit field of the packed word at
// 0x28DC, but only when the matching object is bound; all three set dirty
// bit 37. Each is a BRIDGE between two Acc_82103xxx accessors.
//
//      lwz    r11,12448(r3) ; li r12,1 ; lwz r9,10460(r3)
//      subfic r11,r11,0 ; stw r4,11868(r3) ; rldicr r12,r12,37,63
//      subfe  r11,r11,r11 ; and r11,r11,r4
//      rlwimi r11,r9,0,0,27 ; stw r11,10460(r3)
//      ld     r11,16(r3) ; or r11,r11,r12 ; std r11,16(r3)
//
// `subfic rD,rS,0` followed by `subfe rD,rD,rD` is 0 when rS is zero and all
// ones otherwise -- the same carry trick MATCHED.md records for `addic`/
// `subfe`, one instruction pair earlier in the expression. AND-ing it with
// the argument is `bound ? v : 0`.
//
// The three fields are bits 0..3, 4..7 and 8..11 of one word, and the three
// bound pointers and three recorded values are likewise consecutive, so this
// is one three-slot register.
//
// Both of y1_byte_state.cpp's levers again: MATCHED.md's sub_827FEE48
// address-of lever on the dirty word, and /O2 /Os.

struct FieldState
{
    /* 0x0000 */ char  unk0000[0x10];
    /* 0x0010 */ u64   dirty;
    /* 0x0018 */ char  unk0018[0x28DC - 0x18];
    /* 0x28DC */ u32   word;
    /* 0x28E0 */ char  unk28E0[0x2E5C - 0x28E0];
    /* 0x2E5C */ u32   v0;
    /* 0x2E60 */ u32   v1;
    /* 0x2E64 */ u32   v2;
    /* 0x2E68 */ char  unk2E68[0x30A0 - 0x2E68];
    /* 0x30A0 */ void* bound0;
    /* 0x30A4 */ void* bound1;
    /* 0x30A8 */ void* bound2;
};
ASSERT_OFFSET(FieldState, dirty,  0x0010);
ASSERT_OFFSET(FieldState, word,   0x28DC);
ASSERT_OFFSET(FieldState, v0,     0x2E5C);
ASSERT_OFFSET(FieldState, v2,     0x2E64);
ASSERT_OFFSET(FieldState, bound0, 0x30A0);
ASSERT_OFFSET(FieldState, bound2, 0x30A8);

void SetField0(FieldState* d, u32 v)
{
    d->v0 = v;

    u32 m = d->bound0 ? v : 0;

    /* NEAR MISS, 2 of 14 words: the image merges with
     *     rlwimi r11,r9,0,0,27   (the masked VALUE is the destination)
     * and every spelling here gives
     *     rlwimi r9,r11,0,28,31  (the WORD is the destination).
     * Three were measured -- the mask either side of the `|`, and the value
     * in a local OR-ed in place -- and all three are identical, which is
     * MATCHED.md's result for `or` operand order not being source-readable,
     * reached from the rlwimi side. The other twelve words, both siblings at
     * shifts 4 and 8, and the flag level are all right. */
    d->word = (d->word & ~0x0000000Fu) | (m & 0x0000000Fu);

    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 37);
}

void SetField1(FieldState* d, u32 v)
{
    d->v1 = v;

    u32 m = d->bound1 ? v : 0;

    d->word = (d->word & ~0x000000F0u) | ((m << 4) & 0x000000F0u);

    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 37);
}

void SetField2(FieldState* d, u32 v)
{
    d->v2 = v;

    u32 m = d->bound2 ? v : 0;

    d->word = (d->word & ~0x00000F00u) | ((m << 8) & 0x00000F00u);

    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 37);
}
