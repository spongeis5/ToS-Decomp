#include "types.h"

// sub_8262FF90 -- drop the last reference on a global singleton, clear a
// thread-local pointer and tail-call the teardown. 52 B, 10 callers.
//
//      lis     r11,-32092
//      lwz     r11,-23904(r11)   r11 = g_owner          (a global POINTER)
//      lwz     r10,12(r11)
//      addic.  r10,r10,-1
//      stw     r10,12(r11)       --g_owner->count
//      bgtlr                     still referenced: done
//      lwz     r10,0(r13)        TLS block
//      li      r9,44             TLS slot, LINKER-assigned
//      li      r8,0
//      addi    r3,r11,24         &g_owner->sub
//      stwx    r8,r9,r10         t_current = 0
//      b       0x8291285c        tail call
//
// `lwz rX,0(r13)` plus a bare `li` of a small unfolded constant is
// __declspec(thread) on this compiler; the `li` carries an
// IMAGE_REL_PPC_TOCREL14 for the slot and build.py resolves it from the
// retail word, so our object emits `li r9,0`.
//
// `bgtlr` is SIGNED, so the count is an int; the store of the decremented
// value happens before the test, which is the ordinary `--x` on a field.

struct Owner
{
    /* 0x00 */ char unk0000[0x0C];
    /* 0x0C */ s32  count;
    /* 0x10 */ char unk0010[0x08];
    /* 0x18 */ char sub[4];
};
ASSERT_OFFSET(Owner, count, 0x0C);
ASSERT_OFFSET(Owner, sub,   0x18);

extern Owner* g_owner;

__declspec(thread) void* t_current;

void Teardown(void* sub);

void ReleaseGlobal()
{
    Owner* o = g_owner;
    if (--o->count > 0)
        return;
    t_current = 0;
    Teardown(o->sub);
}
