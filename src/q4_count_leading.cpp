#include "types.h"

// sub_82724588 -- count the leading -1 entries of an array, scanning indices
// 0..count INCLUSIVE. 56 B, 8 callers.
//
//      lwz     r10,8(r3)        n = t->count
//      mr      r11,r3           t moves out of r3 ...
//      li      r3,0             ... because r3 is the RESULT, i
//      cmpwi   cr6,r10,0
//      bltlr   cr6              the `for` rotation guard: 0 <= n
//      lwz     r11,0(r11)       p = t->items   (loaded AFTER the guard)
//  L:  lwz     r9,0(r11)
//      cmpwi   cr6,r9,-1
//      bnelr   cr6              the break, returning i
//      addi    r3,r3,1
//      addi    r11,r11,4
//      cmpw    cr6,r3,r10
//      ble+    cr6,L
//      blr
//
// `bltlr` on a SIGNED compare with r3 already zero is the standard MSVC
// rotation of `for (i = 0; i <= n; i++)`: skip the whole loop when n < 0, and
// the zero it returns is the loop counter's own initial value.
//
// `count` is read ONCE into r10 and never reloaded, so it is a named local;
// `items` is loop-invariant and its load lands in the preheader, which is why
// it sits after the guard even though it is written before the loop.
struct FreeList
{
    /* 0x00 */ s32* items;
    /* 0x04 */ char unk0004[0x04];
    /* 0x08 */ s32  count;
};
ASSERT_OFFSET(FreeList, count, 0x08);

s32 CountLeadingFree(FreeList* t)
{
    s32 n = t->count;
    s32 i;

    for (i = 0; i <= n; i++)
        if (t->items[i] != -1)
            break;

    return i;
}
