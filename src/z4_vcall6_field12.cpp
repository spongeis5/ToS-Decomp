#include "types.h"

// sub_8224BCF8 -- 20 bytes.  Tail-call slot 6 of the first argument's vtable,
// replacing the second argument with a field read out of it.
//
//      lwz     r11,0(r3)        vt = o->vt
//      lwz     r4,12(r4)        arg2 = a->value
//      lwz     r10,24(r11)      slot 24/4 = 6
//      mtctr   r10
//      bctr
//
// The vtable idiom from MATCHED.md's table, with the slot index read off the
// displacement.  r3 is never touched, so the object stays the `this` of the
// call; only the second argument is rewritten, and the source therefore
// passes `a->value` where the wrapper took `a`.
//
// A FRESH register (r10) holds the slot rather than reusing r11 -- the same
// choice sub_82600BB0 makes, and plain /O2.
//
// Nothing here is relocated: 5 of 5 words are compared.

struct Obj;

struct ObjVT
{
    void* (*slot[7])(Obj*, void*);
};

struct Obj
{
    /* 0x00 */ ObjVT* vt;
};

ASSERT_OFFSET(Obj, vt, 0x00);

struct Arg
{
    /* 0x00 */ char  unk0000[0x0C];
    /* 0x0C */ void* value;
};

ASSERT_OFFSET(Arg, value, 0x0C);

void* CallSlot6(Obj* o, Arg* a)
{
    return o->vt->slot[6](o, a->value);
}
