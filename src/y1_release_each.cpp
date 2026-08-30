#include "types.h"

// sub_8261AB90 -- walk an array of items, drop a reference on each item's
// node, and hand the node's payload to a virtual call at zero. 124 B.
// Bridge between 8261AB88 and 8261AC10.
//
//      lwz r11,0(r3) ; li r27,0 ; mr r29,r3 ; mr r28,r27
//      cmpwi cr6,r11,0 ; ble- end ; mr r30,r27
//   L: lwz r11,4(r29) ; lwzx r10,r30,r11 ; lwz r31,20(r10)
//      addi r11,r31,64                        DEAD -- &n->count
//      lwz r11,64(r31) ; addi r9,r11,-1 ; rotlwi r8,r9,0
//      stw r9,64(r31) ; cmpwi cr6,r8,0 ; bne- skip
//      mr r3,r31 ; lwz r4,60(r31) ; bl 0x82600bb0 ; stw r27,60(r31)
// skip:lwz r11,0(r29) ; addi r28,r28,1 ; addi r30,r30,4
//      cmpw cr6,r28,r11 ; blt+ L
//
// Two of MATCHED.md's fingerprints are in the reference drop, and they are
// the whole of it:
//
//  * `addi r11,r31,64` computes an address that is overwritten on the very
//    next instruction. A bare `s32* c = &n->count;` is NOT enough -- it
//    folds into 64(r31) and leaves nothing behind. It needs TWO nesting
//    levels of inlined helper, the outer one taking `&n->count`, which is
//    sub_82164040's and sub_82703E28's shape exactly;
//  * `rotlwi r8,r9,0` is a register copy of a value that is both stored and
//    compared, which is the CSE-copy fingerprint: the source names the count
//    again for the test rather than binding the decremented value to a
//    local. Reading the test through the same pointer (`if (*c == 0)`)
//    instead lets MSVC fuse the whole thing into one `addic.` and loses
//    three words.
//
// The count is reloaded at the loop bottom while the entry test uses the
// first load -- MATCHED.md's "a reload inside a LOOP CONDITION is the normal
// shape", so a plain `i < l->count` produces it.

struct Node8261AB90
{
    /* 0x00 */ char unk0000[0x3C];
    /* 0x3C */ s32  payload;
    /* 0x40 */ s32  count;
};
ASSERT_OFFSET(Node8261AB90, payload, 0x3C);
ASSERT_OFFSET(Node8261AB90, count,   0x40);

struct Item8261AB90
{
    /* 0x00 */ char             unk0000[0x14];
    /* 0x14 */ Node8261AB90*    node;
};
ASSERT_OFFSET(Item8261AB90, node, 0x14);

struct List8261AB90
{
    /* 0x00 */ s32           count;
    /* 0x04 */ Item8261AB90** items;
};
ASSERT_OFFSET(List8261AB90, items, 0x04);

void NodeNotify(Node8261AB90* n, s32 payload);    /* sub_82600BB0 */

static void PutCount(s32* p, s32 v)
{
    *p = v;
}

static void DecCount(s32* p)
{
    PutCount(p, *p - 1);
}

void ReleaseEach(List8261AB90* l)
{
    for (s32 i = 0; i < l->count; i++)
    {
        Node8261AB90* n = l->items[i]->node;

        DecCount(&n->count);

        if (n->count == 0)
        {
            NodeNotify(n, n->payload);
            n->payload = 0;
        }
    }
}
