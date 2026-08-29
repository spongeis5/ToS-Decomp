// sub_82254A88 -- scan a fixed array of object pointers hanging off a global
// manager and return the first whose state word is 6. 84 B, 25 callers.
//
//      lis     r10,-32108
//      li      r11,0                   i = 0
//      lwz     r8,-24180(r10)          m = g_manager
//      lwz     r9,1080(r8)             m->count
//      cmpwi   cr6,r9,0
//      ble-    cr6,ret0
//      addi    r10,r8,1064             &m->items[0]
//  L:  lwz     r7,0(r10)               m->items[i]
//      lwz     r6,764(r7)              ->state
//      cmpwi   cr6,r6,6
//      beq-    cr6,found
//      addi    r11,r11,1
//      addi    r10,r10,4
//      cmpw    cr6,r11,r9
//      blt+    cr6,L
// ret0:li      r3,0
//      blr
// found:
//      addi    r11,r11,266             266 * 4 == 1064, the array offset
//      rlwinm  r10,r11,2,0,29
//      lwzx    r3,r10,r8
//      blr
//
// The found block recomputes the element address from the INDEX rather than
// reusing the walking pointer r10 or the already-loaded r7: it sits outside
// the loop, where strength reduction has not propagated, so the compiler
// folds the 1064-byte field offset into the subscript as `i + 266`. That is
// what says the exit is a separate block and not a `break` with the value
// carried out.
//
// count at 1080 is exactly items[4], so the array is four entries wide.

#include "types.h"

struct Entry
{
    /* 0x000 */ char unk0000[0x2FC];
    /* 0x2FC */ s32  state;
};

ASSERT_OFFSET(Entry, state, 0x2FC);

struct Manager
{
    /* 0x000 */ char   unk0000[0x428];
    /* 0x428 */ Entry* items[4];
    /* 0x438 */ s32    count;
};

ASSERT_OFFSET(Manager, items, 0x428);
ASSERT_OFFSET(Manager, count, 0x438);

extern Manager* g_manager;

Entry* FindEntryInState6()
{
    Manager* m = g_manager;

    for (int i = 0; i < m->count; ++i)
        if (m->items[i]->state == 6)
            return m->items[i];

    return 0;
}
