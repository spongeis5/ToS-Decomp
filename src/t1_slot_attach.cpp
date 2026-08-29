#include "types.h"

// sub_825492F8 -- install an object into a bounds-checked slot array, back-link
// it to the owner, then tail call through its vtable. 100 B, 7 callers.
//
//      mr      r7,r6            park the 4th argument: r6 is about to be
//                               reused for a field, so `d` has to move first
//      cmplwi  cr6,r5,0 ; beq-  -> 37     the object is null
//      cmpwi   cr6,r4,0 ; blt-  -> 37     index negative  (SIGNED)
//      lwz     r11,4(r3)
//      cmpw    cr6,r4,r11 ; bge- -> 37    index past count
//      lwz     r11,20(r3) ; rlwinm r10,r4,2,0,29 ; stwx r5,r11,r10
//      lwz     r9,20(r3)  ; lwzx r8,r9,r10 ; stw r3,12(r8)
//      lwz     r6,20(r3)  ; lwzx r11,r6,r10
//      lwz     r10,0(r11) ; lwz r6,16(r3) ; lwz r5,12(r3)
//      mr      r3,r11 ; lwz r9,0(r10) ; mtctr r9 ; bctr
//   37:
//      li      r3,37 ; blr
//
// Three separate loads of the `slots` field, with a store between each pair:
// the stores could alias, so this is ordinary aliasing rather than the
// CSE-defeat lever, and spelling `o->slots[i]` out at all three uses is what
// produces it.
//
// r4 is NEVER written before the tail call, so the index is still an argument
// of the callee -- the call takes five: (slot, i, o->a, o->b, d).
//
// All three guards branch FORWARD to a shared `li r3,37`, which by the
// branch-direction rule means the failure return is written LAST and the
// positive path is nested.

struct Slot;
struct Owner;

struct SlotVT
{
    /* 0x00 */ int (*attach)(Slot*, int, s32, s32, void*);
};

struct Slot
{
    /* 0x00 */ const SlotVT* vt;
    /* 0x04 */ char          unk0004[0x08];
    /* 0x0C */ Owner*        owner;
};
ASSERT_OFFSET(Slot, vt,    0x00);
ASSERT_OFFSET(Slot, owner, 0x0C);

struct Owner
{
    /* 0x00 */ char  unk0000[0x04];
    /* 0x04 */ s32   count;
    /* 0x08 */ char  unk0008[0x04];
    /* 0x0C */ s32   a;
    /* 0x10 */ s32   b;
    /* 0x14 */ Slot** slots;
};
ASSERT_OFFSET(Owner, count, 0x04);
ASSERT_OFFSET(Owner, a,     0x0C);
ASSERT_OFFSET(Owner, b,     0x10);
ASSERT_OFFSET(Owner, slots, 0x14);

int AttachSlot(Owner* o, int i, Slot* s, void* d)
{
    if (s != 0 && i >= 0 && i < o->count)
    {
        o->slots[i] = s;
        o->slots[i]->owner = o;
        return o->slots[i]->vt->attach(o->slots[i], i, o->a, o->b, d);
    }
    return 37;
}
