// sub_827103D8 -- dispatch to a handler through its vtable, or tail-call a
// default when there is none. 52 B, 5 callers.
//
//      lwz     r10,16(r3)         h->owner              (+0x10)
//      mr      r11,r4             save argument 2
//      lwz     r3,20(r10)         t = owner->handler    (+0x14)
//      cmplwi  cr6,r3,0
//      bne-    cr6,0x827103F8
//      mr      r4,r5              argument 3 -> 2
//      mr      r3,r11             argument 2 -> 1
//      b       0x826A9870
//  827103F8:
//      lwz     r10,0(r3)
//      mr      r4,r11
//      lwz     r11,4(r10)         slot 4/4 = 1
//      mtctr   r11
//      bctr
//
// `bne-` jumps AWAY to the virtual call, so the DEFAULT path is the
// fall-through and is written first -- the early-return spelling
// `if (t == 0) return Default(a, b);`. Writing the virtual call first inverts
// the compare and displaces everything after it.
//
// `mr r11,r4` at the top is argument 2 being saved before r3 and r4 are both
// overwritten; it happens before the loads because the load into r3 kills the
// object pointer. Argument 3 reaches the default in r4 and never reaches the
// virtual call at all, so the two callees take different argument lists.
//
// The handler pointer is loaded straight into r3 with no copy, which is the
// named-local spelling (compare src/a_vcall4_or_neg1.cpp, where repeating the
// member expression forces a scratch register and a copy back into r3).
//
// NEEDS /O2 /Os. At plain /O2 the instructions and their order are already
// right and exactly two words differ: the vtable slot lands in a FRESH r9
// where retail reuses r11 -- the register-coalescing signature MATCHED.md
// records. r11 is the register argument 2 was saved in, and it is dead by
// then, so /Os writes the slot back into it.
//
// One word is relocated (the `b` to 826A9870), so 12 of 13 are compared.

#include "types.h"

struct Handler;

struct HandlerVT
{
    int (*slot[2])(Handler*, void*);
};

struct Handler
{
    /* 0x00 */ HandlerVT* vt;
};

ASSERT_OFFSET(Handler, vt, 0x00);

struct HandlerOwner
{
    /* 0x00 */ char     unk0000[0x14];
    /* 0x14 */ Handler* handler;
};

ASSERT_OFFSET(HandlerOwner, handler, 0x14);

struct Host
{
    /* 0x00 */ char          unk0000[0x10];
    /* 0x10 */ HandlerOwner* owner;
};

ASSERT_OFFSET(Host, owner, 0x10);

int DefaultHandle(void* a, void* b);

int Dispatch(Host* h, void* a, void* b)
{
    Handler* t = h->owner->handler;
    if (t == 0)
        return DefaultHandle(a, b);
    return t->vt->slot[1](t, a);
}
