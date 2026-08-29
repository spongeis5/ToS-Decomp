// sub_825ACB20 -- clear one field and add a reference to another, returning
// a zero status. 40 bytes, 3 callers.
//
//      li      r10,0
//      lwz     r11,12(r3)
//      stw     r10,44(r3)
//      cmplwi  cr6,r11,0
//      li      r3,0
//      beqlr   cr6
//      lwz     r10,8(r11)
//      addi    r10,r10,1
//      stw     r10,8(r11)
//      blr
//
// `li r3,0` is materialised BEFORE the guard, which is what lets the guard be
// a `beqlr` at all -- the return value has to already be in place for a
// conditional return to work. That says there is one `return 0` at the end
// and the null test is an `if` around the increment, not an early return with
// its own value.
//
// The load of +12 is issued before the store to +44 and there is nothing to
// stop MSVC moving it either way; the STORE order is what carries the source
// order, and there is only one store before the guard.
//
// Nothing is relocated: 10 of 10 words are compared.

#include "types.h"

struct RefCounted
{
    /* 0x00 */ u8  unk0000[8];
    /* 0x08 */ s32 refs;
};

ASSERT_OFFSET(RefCounted, refs, 8);

struct Binding
{
    /* 0x00 */ u8          unk0000[0x0C];
    /* 0x0C */ RefCounted* target;
    /* 0x10 */ u8          unk0010[0x1C];
    /* 0x2C */ s32         cursor;
};

ASSERT_OFFSET(Binding, target, 0x0C);
ASSERT_OFFSET(Binding, cursor, 44);

int RewindBinding(Binding* b)
{
    b->cursor = 0;

    RefCounted* t = b->target;
    if (t != 0)
        t->refs = t->refs + 1;

    return 0;
}
