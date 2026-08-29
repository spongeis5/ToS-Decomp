// sub_82265E90 -- bounds-checked element fetch. 44 B, 3 callers.
//
//      cmpwi  cr6,r4,0
//      blt-   cr6,zero
//      lwz    r11,72(r3)
//      cmpw   cr6,r4,r11
//      bge-   cr6,zero
//      lwz    r11,68(r3)
//      rlwinm r10,r4,2,0,29
//      lwzx   r3,r11,r10
//      blr
// zero:li     r3,0
//      blr
//
// Both compares are SIGNED (`cmpwi`, `cmpw`), so the index and the count are
// `int` and the lower bound is a real test rather than a formality -- an
// unsigned index would need only the one comparison.
//
// Both guards branch FORWARD to a single `li r3,0` planted after the return,
// which by the branch-direction rule means the failure value is written
// LAST and the fetch is the fall-through.  Two flat `if (...) return 0;`
// guards would instead plant a private zero after the first test.
//
// `lwzx r3,r11,r10` has the BASE in rA, which is the free-function form for
// an array member at a non-zero offset -- here +68, with the count at +72.

#include "types.h"

struct BoundedArray
{
    /* 0x00 */ char   unk0000[0x44];
    /* 0x44 */ void** items;
    /* 0x48 */ s32    count;
};
ASSERT_OFFSET(BoundedArray, items, 0x44);
ASSERT_OFFSET(BoundedArray, count, 0x48);

void* ElementAt(BoundedArray* a, int i)
{
    if (i >= 0 && i < a->count)
        return a->items[i];

    return 0;
}
