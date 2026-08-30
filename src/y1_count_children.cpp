#include "types.h"

// sub_82696938 -- count how many indices the object's own iterator yields.
// 116 B.  Bridge between TypeId_82696930 and Acc_826969B0.
//
//      lwz  r11,0(r3) ; li r30,0 ; lwz r10,8(r11) ; bctrl    First()
//      cmpwi cr6,r3,-1 ; beq- done
//   L: lwz  r11,0(r31)          the vtable is RELOADED every iteration
//      mr   r4,r3 ; mr r3,r31 ; addi r30,r30,1
//      lwz  r10,12(r11) ; bctrl                              Next(i)
//      cmpwi cr6,r3,-1 ; bne+ L
//      mr   r3,r30
//
// A peeled test in front and a copy at the bottom is the rotated `while`,
// not a `do/while`; the reload of the vtable inside the loop says the
// dispatch is spelled out at both call sites rather than held in a local.

struct Coll;

struct CollVT
{
    void (*d0)(Coll*);
    void (*d1)(Coll*);
    s32  (*first)(Coll*);
    s32  (*next)(Coll*, s32);
};

struct Coll
{
    /* 0x00 */ CollVT* vt;
};
ASSERT_OFFSET(Coll, vt, 0x00);

s32 CountChildren(Coll* c)
{
    s32 n = 0;
    s32 i = c->vt->first(c);

    while (i != -1)
    {
        n++;
        i = c->vt->next(c, i);
    }
    return n;
}
