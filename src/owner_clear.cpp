// sub_82677028 and sub_82677040 -- a matched pair of clear-and-tail-call
// methods on one object.
//
// Merged into one translation unit, and onto ONE struct, on evidence:
//
//   * they sit 4 bytes apart (82677028 + 20 = 8267703C, next starts 82677040)
//   * both take the same pointer in r3 and read a pointer from +0x84
//   * they clear adjacent fields, +0x88 and +0x8C
//
// Two functions this close, reading the same field of the same argument and
// writing neighbouring fields, are methods of one class. This is the only
// type identity among the 15 matched functions that the evidence actually
// supports -- see MATCHED.md. The vtable-storing pair 826FE5B8/826FE5C8 is
// adjacent too, but a vtable pointer at +0 is true of every polymorphic
// class and says nothing about identity.

#include "types.h"

struct Owner
{
    /* 0x00 */ char  unk0000[0x84];
    /* 0x84 */ void* obj;
    /* 0x88 */ s32   flag88;
    /* 0x8C */ s32   flag8C;
};

ASSERT_OFFSET(Owner, obj,    0x84);
ASSERT_OFFSET(Owner, flag88, 0x88);
ASSERT_OFFSET(Owner, flag8C, 0x8C);
// No ASSERT_SIZE: nothing observed bounds this object, and asserting a size
// from a guess would compile happily while being wrong.

void Handle(void*);
void HandleOther(void*);

// sub_82677028, 20 bytes, 25 callers.
//
//      mr      r11,r3          keep this
//      lwz     r3,132(r3)      argument for the call
//      li      r10,0
//      stw     r10,136(r11)    this->flag88 = 0
//      b       0x826E5210
//
// The load of obj is emitted BEFORE the store, which is why `this` has to
// survive in r11.
void ClearAndHandle(Owner* o)
{
    o->flag88 = 0;
    Handle(o->obj);
}

// sub_82677040, 20 bytes, 25 callers. The same shape against flag8C.
void ClearAndHandleOther(Owner* o)
{
    o->flag8C = 0;
    HandleOther(o->obj);
}
