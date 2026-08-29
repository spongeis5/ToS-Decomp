// sub_82265EC0 -- index of an element, or -1. 68 B, 3 callers.
//
//      lwz   r9,72(r3)
//      mr    r10,r3
//      li    r3,-1
//      li    r11,0
//      cmpwi cr6,r9,0
//      blelr cr6
//      lwz   r10,68(r10)
// L:   lwz   r8,0(r10)
//      cmplw cr6,r8,r4
//      beq-  cr6,found
//      addi  r11,r11,1
//      addi  r10,r10,4
//      cmpw  cr6,r11,r9
//      blt+  cr6,L
//      blr
// found:mr   r3,r11
//
// The same object as sub_82265E90 (src/l40_bounds_at.cpp), 0x30 earlier:
// items at +68, count at +72, signed count.  Two functions over one layout
// at adjacent addresses is what makes the layout worth trusting.
//
// The count is loaded and tested BEFORE the items pointer is loaded, which
// is what spelling `a->count` and `a->items` out gives -- naming the array
// in a local first is the documented way to pull that load across the guard.
//
// The loop carries both an induction pointer and an index because the INDEX
// is the return value; MSVC strength-reduced the subscript and kept `i`
// only for the `mr r3,r11` at the hit.
//
// `li r3,-1` sits above the guard and serves both failures -- the empty case
// and the fall-out -- so the not-found value is materialised once, in the
// entry block.

#include "types.h"

struct BoundedArray
{
    /* 0x00 */ char   unk0000[0x44];
    /* 0x44 */ void** items;
    /* 0x48 */ s32    count;
};
ASSERT_OFFSET(BoundedArray, items, 0x44);
ASSERT_OFFSET(BoundedArray, count, 0x48);

int IndexOf(BoundedArray* a, void* v)
{
    int r = -1;
    int i;

    for (i = 0; i < a->count; i++)
    {
        if (a->items[i] == v)
        {
            r = i;
            break;
        }
    }

    return r;
}
