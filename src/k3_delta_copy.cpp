// sub_8282E258 -- copy a run of ints into a second object, writing each
// element and, one slot on, its forward difference. 92 B, 5 callers.
//
//      lwz  r11,40(r4) ; lwz r10,36(r4) ; subf r8,r10,r11 ; stw r8,56(r3)
//      lwz  r7,44(r4)  ; stw r7,60(r3)
//      lwz  r6,44(r4)  ; cmpwi cr6,r6,0 ; blelr cr6      RELOADED
//      addi r10,r3,64  ; addi r11,r4,48
//   L: lwz  r8,0(r11)  ; addi r9,r9,1 ; stw r8,0(r10)
//      lwz  r7,0(r11)                                    RELOADED
//      lwzu r8,4(r11)  ; subf r6,r7,r8 ; stwu r6,4(r10)
//      lwz  r5,44(r4)  ; cmpw cr6,r9,r5 ; blt+ L         RELOADED
//
// THREE reloads of 44(r4) and one of the source element, all of them
// aliasing: the destination is a different pointer than the source, so every
// store to it invalidates everything read through the other. Writing the
// count into a local would remove all three, so it is spelled out at each
// use.
//
// `subf rD,rA,rB` computes rB - rA, so `subf r8,r10,r11` is [40] - [36] and
// `subf r6,r7,r8` is s[i+1] - s[i]. Both orders are readable and neither is
// a guess.
//
// The two induction registers are the tell for the loop body's shape: r11
// advances by ONE element with `lwzu` after reading s[i] twice and s[i+1]
// once, and r10 advances by one element with `stwu` after storing at d[i]
// and d[i+1]. So the body writes d[i] and then d[i+1], and every d[i+1]
// except the last is overwritten by the next iteration.
//
// `blelr` is the for-loop's entry test after rotation, not a separate guard.

#include "types.h"

struct DeltaSrc
{
    /* 0x00 */ u8  unk0000[0x24];
    /* 0x24 */ s32 lo;
    /* 0x28 */ s32 hi;
    /* 0x2C */ s32 count;
    /* 0x30 */ s32 values[1];
};
ASSERT_OFFSET(DeltaSrc, lo, 0x24);
ASSERT_OFFSET(DeltaSrc, hi, 0x28);
ASSERT_OFFSET(DeltaSrc, count, 0x2C);
ASSERT_OFFSET(DeltaSrc, values, 0x30);

struct DeltaDst
{
    /* 0x00 */ u8  unk0000[0x38];
    /* 0x38 */ s32 span;
    /* 0x3C */ s32 count;
    /* 0x40 */ s32 values[1];
};
ASSERT_OFFSET(DeltaDst, span, 0x38);
ASSERT_OFFSET(DeltaDst, count, 0x3C);
ASSERT_OFFSET(DeltaDst, values, 0x40);

void DeltaCopy(DeltaDst* d, DeltaSrc* s)
{
    d->span = s->hi - s->lo;
    d->count = s->count;

    for (s32 i = 0; i < s->count; i++)
    {
        d->values[i] = s->values[i];
        d->values[i + 1] = s->values[i + 1] - s->values[i];
    }
}
