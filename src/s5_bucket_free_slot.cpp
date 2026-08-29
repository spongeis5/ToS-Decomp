#include "types.h"

// sub_8219EBB8 -- the sibling of sub_8219EB58 (c_bucket_find.cpp), which ends
// exactly here (8219EB58 + 96 = 8219EBB8), so the two are one translation
// unit and this is /O2 as well. Same open-addressed table, same power-of-two
// capacity, same wrap-all-the-way-round probe -- but it returns the ADDRESS
// of the first EMPTY slot rather than the entry stored in a matching one.
// 84 B, 7 callers.
//
//   lwz    r9,0(r3)           n = t->capacity
//   mr     r11,r3             t out of r3 ...
//   li     r3,0               ... because r3 holds the result from the start
//   addi   r10,r9,-1
//   and    r8,r10,r4          start = (n - 1) & key
//   lwz    r7,4(r11)          t->slots, hoisted
//   mr     r11,r8             i = start
// L:rlwinm r10,r11,2,0,29     i * 4
//   add    r10,r10,r7         &t->slots[i]   -- MATERIALISED, not lwzx
//   lwz    r6,0(r10)          *p
//   cmplwi cr6,r6,0
//   beq-   cr6,found
//   addi   r11,r11,1
//   subfc  r10,r9,r11         CA = (i >= n) unsigned
//   subfe  r5,r6,r6           0 when CA, else -1
//   and    r11,r5,r11         i = (i >= n) ? 0 : i
//   cmplw  cr6,r11,r8
//   bne+   cr6,L
//   blr                       falls out with r3 still 0
// found:
//   mr     r3,r10
//   blr
//
// The one instruction that differs from c_bucket_find is `add r10,r10,r7` +
// `lwz r6,0(r10)` where that one has `lwzx r6,r10,r9`. An lwzx never leaves
// the address in a register, and this function's RESULT is the address -- so
// the source names the pointer and dereferences it, instead of subscripting.
//
// Everything else is carried over from c_bucket_find and is measured there:
// ONE result variable with a `break` (not two returns) is what forces `t` out
// of r3 into r11 and pays for the out-of-line `mr r3,r10`, and `start` is
// computed first with `i` initialised from it, which is the direction the
// `and r8,... ; mr r11,r8` copy runs.
//
// The wrap is the same subfc/subfe borrow trick, reusing r6 -- the loaded
// slot -- as both sources of the subfe, which is legal because only the carry
// matters and r6 is dead on that edge.

struct FreeSlotTable
{
    u32    capacity;
    void** slots;
};
ASSERT_OFFSET(FreeSlotTable, slots, 0x04);

void** BucketFindFree(FreeSlotTable* t, u32 key)
{
    u32    n = t->capacity;
    u32    start = (n - 1) & key;
    u32    i = start;
    void** found = 0;

    do
    {
        void** p = &t->slots[i];
        if (*p == 0)
        {
            found = p;
            break;
        }
        ++i;
        if (i >= n)
            i = 0;
    } while (i != start);

    return found;
}
