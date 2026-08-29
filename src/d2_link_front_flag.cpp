// sub_827FEE48 -- push a node on the front of an owner's intrusive doubly
// linked list and set a flag bit. 44 B, 5 callers.
//
//      lwz     r11,2832(r4)       o->head        (+0xB10)
//      cmplwi  cr6,r11,0
//      beq-    cr6,0x827FEE60
//      stw     r3,120(r11)        o->head->prev = n     (+0x78)
//      lwz     r11,2832(r4)       RELOAD of o->head
//      stw     r11,116(r3)        n->next = o->head     (+0x74)
//  827FEE60:
//      stw     r3,2832(r4)        o->head = n
//      lwz     r11,2844(r4)       o->flags       (+0xB1C)
//      ori     r11,r11,1024       |= 0x400
//      stw     r11,2844(r4)
//      blr
//
// r3 is the NODE and r4 the owner, so the node is the first parameter and
// this is a free function rather than a member of the owner.
//
// The RELOAD of `o->head` after the store through it is the tell that the
// source spelled `o->head` out at both uses rather than naming it: a local
// could not be invalidated by a store through it, and MSVC would keep it in a
// register (MATCHED.md, "when a target reloads a field it already had, and
// nothing stored in between, the source spelled the two reads differently" --
// here there IS a store in between and it may alias, which is the same
// conclusion reached the easy way).
//
// `ori` with no `rlwinm` around it, and no sign extension anywhere, so the
// flag word is a plain u32.
//
// Nothing is relocated; all 11 words are compared.

#include "types.h"

struct LNode
{
    /* 0x00 */ char   unk0000[0x74];
    /* 0x74 */ LNode* next;
    /* 0x78 */ LNode* prev;
};

ASSERT_OFFSET(LNode, next, 0x74);
ASSERT_OFFSET(LNode, prev, 0x78);

struct LOwner
{
    /* 0x000 */ char   unk0000[0xB10];
    /* 0xB10 */ LNode* head;
    /* 0xB14 */ char   unkB14[0x08];
    /* 0xB1C */ u32    flags;
};

ASSERT_OFFSET(LOwner, head,  0xB10);
ASSERT_OFFSET(LOwner, flags, 0xB1C);

void LinkFront(LNode* n, LOwner* o)
{
    if (o->head)
    {
        o->head->prev = n;
        n->next = o->head;
    }
    o->head = n;
    o->flags |= 0x400;
}
