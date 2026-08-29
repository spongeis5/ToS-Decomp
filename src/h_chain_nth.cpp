#include "types.h"

// sub_826C0F50 -- index a chain of variable-length blocks from the END.
// 100 B, 14 callers.  Sits 40 bytes after sub_826C0F28 (g_sum_chain.cpp) and
// opens with that function inlined verbatim, same struct.
//
//      lwz     r11,4(r3)        p = n->next
//      lwz     r9,28(r3)        total = n->value
//      cmplwi  cr6,r11,0
//      beq-    cr6,after
//  L:  lwz     r10,28(r11)
//      lwz     r11,4(r11)
//      add     r9,r10,r9
//      cmplwi  cr6,r11,0
//      bne+    cr6,L
// after:
//      mr      r10,r3           q = n     (r3 stays live: the fallback reads it)
//      subf    r11,r9,r4        r = index - total
//  M:  lwz     r9,28(r10)
//      add.    r11,r9,r11       r += q->value, sets CR0
//      bge-    found
//      lwz     r10,4(r10)       q = q->next
//      cmplwi  cr6,r10,0
//      bne+    cr6,M
//      lwz     r3,24(r3)        ran off the end: n->items
//      blr
// found:
//      rlwinm  r9,r11,1,0,30
//      lwz     r10,24(r10)
//      add     r11,r11,r9       r*3
//      rlwinm  r11,r11,3,0,28   r*24
//      add     r3,r11,r10
//      blr
//
// The second loop's top is a branch target reached by FALL-THROUGH with no
// peeled copy of the null test in front of it, so it is a do/while -- the
// first loop, which does have the peeled copy, is a plain while.
//
// `(x + x*2) * 8` is the 24-byte stride idiom, so the block payload is an
// array of 24-byte elements at offset 0x18.

struct NthItem
{
    char unk0000[24];
};
ASSERT_SIZE(NthItem, 24);

struct NthNode
{
    /* 0x00 */ char     unk0000[0x04];
    /* 0x04 */ NthNode* next;
    /* 0x08 */ char     unk0008[0x10];
    /* 0x18 */ NthItem* items;
    /* 0x1C */ s32      value;
};

ASSERT_OFFSET(NthNode, next,  0x04);
ASSERT_OFFSET(NthNode, items, 0x18);
ASSERT_OFFSET(NthNode, value, 0x1C);

NthItem* ChainNth(NthNode* n, s32 index)
{
    NthNode* p     = n->next;
    s32      total = n->value;

    while (p)
    {
        total += p->value;
        p = p->next;
    }

    NthNode* q = n;
    s32      r = index - total;

    do
    {
        r += q->value;
        if (r >= 0)
            return q->items + r;
        q = q->next;
    } while (q != 0);

    return n->items;
}
