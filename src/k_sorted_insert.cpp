#include "types.h"

// sub_825FEF00 -- insert a pointer into a priority-ordered fixed array.
// 124 B, 12 callers.
//
//      lwz     r9,64(r3)      n = count
//      li      r11,0          i = 0
//      cmpwi   cr6,r9,0
//      ble-    cr6,placed
//      lwz     r8,28(r4)      key = it->prio      (hoisted)
//      mr      r10,r3
//  L:  lwz     r7,0(r10)
//      lwz     r6,28(r7)
//      cmpw    cr6,r8,r6
//      blt-    cr6,placed     break
//      lwz     r7,64(r3)      count RELOADED
//      addi    r11,r11,1
//      addi    r10,r10,4
//      cmpw    cr6,r11,r7
//      blt+    cr6,L
// placed:
//      cmpw    cr6,r9,r11
//      ble-    cr6,store
//      subf    r8,r11,r9      n - i
//      rlwinm  r10,r9,2,0,29
//      add     r10,r10,r3     &items[n]
//      mtctr   r8
//  S:  lwz     r9,-4(r10)
//      stw     r9,0(r10)
//      addi    r10,r10,-4
//      bdnz+   S
// store:
//      rlwinm  r11,r11,2,0,29
//      stwx    r4,r11,r3      items[i] = it
//      lwz     r11,64(r3)
//      addi    r10,r11,1
//      stw     r10,64(r3)     ++count
//
// The array is at offset 0 -- `mr r10,r3` walks it and `stwx r4,r11,r3` stores
// into it with `this` as the base.
//
// `cmpwi`/`cmpw` throughout: the count and the index are signed.
struct Entry
{
    /* 0x00 */ char unk0000[28];
    /* 0x1C */ s32  prio;
};
ASSERT_OFFSET(Entry, prio, 28);

struct SortedList
{
    /* 0x00 */ Entry* items[16];
    /* 0x40 */ s32    count;

    void Insert(Entry* it);
};
ASSERT_OFFSET(SortedList, count, 0x40);

void SortedList::Insert(Entry* it)
{
    s32 i = 0;
    s32 j;

    while (i < count)
    {
        if (it->prio < items[i]->prio)
            break;
        ++i;
    }

    for (j = count; j > i; --j)
        items[j] = items[j - 1];

    items[i] = it;
    ++count;
}
