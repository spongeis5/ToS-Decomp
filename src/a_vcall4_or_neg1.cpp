// sub_8224E178 -- 40 bytes, 41 callers. Virtual call through a member
// pointer, or -1 when the member is null.
//
//      lwz     r11,4(r3)
//      cmplwi  cr6,r11,0
//      beq-    cr6,0x8224E198
//      rotlwi  r3,r11,0          register copy, rlwinm rD,rS,0,0,31 form
//      lwz     r11,0(r3)
//      lwz     r10,16(r11)       slot 16/4 = 4
//      mtctr   r10
//      bctr
//  8224E198:
//      li      r3,-1
//      blr
//
// Two things had to be right.
//
// 1. BRANCH POLARITY. beq- jumps AWAY to the -1, so the virtual call is the
//    fall-through and is written FIRST. `if (!t) return -1;` first inverts
//    the compare and loses every word after it.
//
// 2. THE MEMBER EXPRESSION IS NOT BOUND TO A LOCAL, and that is the whole
//    difference between 0/10 and 10/10 here. Written as
//
//        Target* t = h->target;
//        if (t) return t->vt->slot[4](t);
//
//    MSVC allocates the loaded pointer straight into r3, tests r3, and never
//    emits a move -- 36 bytes, nine words wrong, one word short. Written with
//    `h->target` spelled out at all three uses, the compiler CSEs it into a
//    scratch (r11), tests the scratch, and materialises the argument register
//    with a separate copy -- which is exactly the target, 40 bytes, 10/10.
//
//    This is the inverse of the usual advice. A named local is a HINT that
//    the value belongs in the register it will be used from; repeating the
//    subexpression is a hint that it belongs in a temporary. When a target
//    carries an apparently pointless `mr`/`rotlwi` into an argument register,
//    try un-naming the local before anything else.
//
// Nothing is relocated; all 10 words are compared.

#include "types.h"

struct Target;

struct TargetVT
{
    int (*slot[5])(Target*);
};

struct Target
{
    /* 0x00 */ TargetVT* vt;
};

ASSERT_OFFSET(Target, vt, 0x00);

struct Holder
{
    /* 0x00 */ char    unk0000[0x04];
    /* 0x04 */ Target* target;
};

ASSERT_OFFSET(Holder, target, 0x04);

int QueryTarget(Holder* h)
{
    if (h->target)
        return h->target->vt->slot[4](h->target);
    return -1;
}
