#include "types.h"

// sub_826C0F28 -- walk a singly linked chain and total one field. 40 B, 16 callers.
//
//      lwz     r11,4(r3)        p = n->next
//      lwz     r3,28(r3)        total = n->value      (r3 becomes the accumulator)
//      cmplwi  cr6,r11,0
//      beqlr   cr6
//  L:  lwz     r10,28(r11)
//      lwz     r11,4(r11)       value read BEFORE the pointer is advanced
//      add     r3,r10,r3
//      cmplwi  cr6,r11,0
//      bne+    cr6,L
//      blr
//
// cmplwi on the link says pointer. Two copies of the null test -- one ahead of
// the loop reached by fall-through, one at the bottom -- is the rotated `while`
// the target already is, so no do/while is needed here.

struct SumNode
{
    /* 0x00 */ char     unk0000[0x04];
    /* 0x04 */ SumNode* next;
    /* 0x08 */ char     unk0008[0x14];
    /* 0x1C */ s32      value;
};

ASSERT_OFFSET(SumNode, next,  0x04);
ASSERT_OFFSET(SumNode, value, 0x1C);

s32 SumChain(SumNode* n)
{
    SumNode* p     = n->next;
    s32      total = n->value;

    while (p)
    {
        total += p->value;
        p = p->next;
    }
    return total;
}
