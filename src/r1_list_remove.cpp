#include "types.h"

// sub_825FEFC8 -- remove a pointer from a fixed array, shifting the tail down.
// 132 B, 8 callers.  The exact mirror of src/k_sorted_insert.cpp
// (sub_825FEF00, 0x4C bytes earlier, /O2) and the same object layout:
// items at +0, a signed count at +0x40.
//
//      lwz     r9,64(r3)      n = count            <- ENTRY load, kept live
//      li      r11,0          i = 0
//      cmpwi   cr6,r9,0
//      ble-    cr6,found      empty: skip the search
//      mr      r10,r3         p = &items[0]
//  L:  lwz     r8,0(r10)      items[i]
//      cmplw   cr6,r4,r8      UNSIGNED -- a pointer compare
//      beq-    cr6,found      break
//      lwz     r8,64(r3)      count RELOADED for the loop condition
//      addi    r11,r11,1
//      addi    r10,r10,4
//      cmpw    cr6,r11,r8
//      blt+    cr6,L
// found:
//      cmpw    cr6,r11,r9     against the ENTRY load
//      bgelr   cr6            not present: nothing to do
//      addi    r10,r9,-1
//      stw     r10,64(r3)     --count
//      cmpw    cr6,r11,r10    store-to-load forwarded
//      bge-    cr6,tail
//      rlwinm  r10,r11,2,0,29
//      add     r10,r10,r3
//      addi    r10,r10,-4     biased so stwu can do both
//  S:  lwz     r9,8(r10)      items[i + 1]
//      addi    r11,r11,1
//      stwu    r9,4(r10)      items[i] = ...
//      lwz     r8,64(r3)      count RELOADED again
//      cmpw    cr6,r11,r8
//      blt+    cr6,S
// tail:lwz     r11,64(r3)
//      li      r10,0
//      rlwinm  r9,r11,2,0,29
//      stwx    r10,r9,r3      items[count] = 0
//
// Three separate reloads of `count` and one entry load kept live is the shape
// MATCHED.md records for sub_825FEF00: a reload inside a LOOP CONDITION is
// the normal shape, and the entry load survives for the uses outside a loop.
//
// `stwx r10,r9,r3` puts the INDEX in rA with the array at offset 0, which is
// the member-function flavour (see the lwzx operand-order table), so this is
// written as a member exactly as its sibling is.

struct Entry;

struct SortedList
{
    /* 0x00 */ Entry* items[16];
    /* 0x40 */ s32    count;

    void Remove(Entry* it);
};
ASSERT_OFFSET(SortedList, count, 0x40);

void SortedList::Remove(Entry* it)
{
    s32 i = 0;

    while (i < count)
    {
        if (it == items[i])
            break;
        ++i;
    }

    if (i >= count)
        return;

    --count;

    while (i < count)
    {
        items[i] = items[i + 1];
        ++i;
    }

    items[count] = 0;
}
