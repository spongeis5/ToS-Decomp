// sub_827249B8 -- fill count+1 slots with -1 and reset a cursor. 68 B,
// 3 callers.
//
//      lwz    r11,8(r3)
//      li     r7,0
//      addic. r9,r11,1
//      mr     r10,r7
//      ble-   done
//      mr     r11,r7
//      li     r8,-1
// L:   lwz    r9,0(r3)            the base, RELOADED every iteration
//      addi   r10,r10,1
//      stwx   r8,r11,r9
//      addi   r11,r11,4
//      lwz    r9,8(r3)            and so is the count
//      addi   r6,r9,1
//      cmpw   cr6,r10,r6
//      blt+   cr6,L
// done:stw    r7,4(r3)
//
// BOTH RELOADS ARE ALIASING, not a missed CSE: the loop stores through
// `o->data`, and a write through an `s32*` can land on `o->data` or
// `o->count` as far as MSVC can prove.  So every iteration re-reads both,
// and `count + 1` is recomputed with them.  Copying either into a local
// would remove the reloads and is what NOT to write here.
//
// `addic. r9,r11,1` is the peeled first test: it forms count+1 and sets CR0
// in one instruction, so the loop is the ordinary rotated `for` over
// `i < count + 1` -- one MORE than the count, which is a slot per entry plus
// a terminator.
//
// The cursor store at +4 is after the loop in source order as well as in
// the emitted code; it uses the same zero the loop's index started from.

#include "types.h"

struct SlotTable
{
    /* 0x00 */ s32* data;
    /* 0x04 */ s32  cursor;
    /* 0x08 */ s32  count;
};
ASSERT_OFFSET(SlotTable, data,   0x00);
ASSERT_OFFSET(SlotTable, cursor, 0x04);
ASSERT_OFFSET(SlotTable, count,  0x08);

void ResetSlots(SlotTable* t)
{
    int i;

    for (i = 0; i < t->count + 1; i++)
        t->data[i] = -1;

    t->cursor = 0;
}
