// sub_827FEE48 -- push a node on the front of an owner's intrusive doubly
// linked list and set a flag bit. 44 B, 5 callers.
//
//      lwz     r11,2832(r4)       *pp            (+0xB10)
//      cmplwi  cr6,r11,0
//      beq-    cr6,0x827FEE60
//      stw     r3,120(r11)        (*pp)->prev = n       (+0x78)
//      lwz     r11,2832(r4)       RELOAD of *pp
//      stw     r11,116(r3)        n->next = *pp         (+0x74)
//  827FEE60:
//      stw     r3,2832(r4)        *pp = n
//      lwz     r11,2844(r4)       o->flags       (+0xB1C)
//      ori     r11,r11,1024       |= 0x400
//      stw     r11,2844(r4)
//      blr
//
// r3 is the NODE and r4 the owner, so the node is the first parameter and
// this is a free function rather than a member of the owner.
//
// NEEDS /O2 /Os -- at plain /O2 the `ori` and the store after it take a fresh
// r10 where retail reuses r11, the register-coalescing signature.
//
// THE SCHEDULE IS WHAT COSTS THE WORK. Written with `o->head` as an ordinary
// member, every shape is 9 of 11 with the last two words TRANSPOSED: MSVC
// hoists the flags LOAD above the head STORE, because two constant offsets
// off the same base provably cannot alias. 28 of the 72 flag combinations
// tools/flagsweep.py sweeps give that same 9 of 11 -- including /Ou, the
// PowerPC prescheduling switch -- so it is not on the flag axis, and neither
// is it on nine other source axes: `|= ` versus `= x |`, a signed flags
// field, `volatile` on the flags (all still 9 of 11), the member form, an
// inlined helper for either half, a nested list sub-struct, `1 << 10`, and a
// named local for the flags word.
//
// What fixes it is reaching the head through a POINTER. Four spellings are
// 11 of 11 -- a `LNode**` local, that local declared up front, a `LNode*&`
// reference, and a static helper taking `LNode**` -- and they are one
// change: once the store goes through an address that was TAKEN, MSVC will
// no longer move the flags load across it. The written-out form is used here
// because it is one function; the helper
//
//     static void PushFront(LNode** head, LNode* n);
//     PushFront(&o->head, n); o->flags |= 0x400;
//
// compiles to the same 44 bytes and is the likelier retail spelling.
//
// The RELOAD of `*pp` after the store through it comes from the same file:
// spelling `*pp` out at all three uses forces it, and the control is the
// helper written with `LNode* h = *head;` -- naming it removes the reload and
// drops to 3 of 10, four bytes short.
//
// `ori` with no `rlwinm` around it and no sign extension anywhere, so the
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
    LNode** pp = &o->head;

    if (*pp)
    {
        (*pp)->prev = n;
        n->next = *pp;
    }
    *pp = n;

    o->flags |= 0x400;
}
