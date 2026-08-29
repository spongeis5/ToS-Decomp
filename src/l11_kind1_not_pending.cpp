// sub_822598E0 -- "kind is 1 and nothing is pending". 72 B, 4 callers.
//
//      lwz     r11,8(r3)
//      lwz     r10,2240(r11)
//      cmpwi   cr6,r10,1
//      bne-    cr6,zero
//      lwz     r10,2264(r11) ; cmpwi ; bne- one
//      lwz     r11,2268(r11) ; cmpwi ; li r11,0 ; beq- done
// one: li      r11,1
// done:clrlwi  r11,r11,24        <- the inlined bool helper, materialised
//      li      r3,1
//      cmplwi  cr6,r11,0
//      beqlr   cr6
// zero:li      r3,0
//      blr
//
// The same object src/i_state_idle.cpp and src/eq1_2260.cpp reach through a
// pointer at +8: kind at 0x8C0 (signed, cmpwi), the pending flags at 0x8D8
// and 0x8DC.
//
// ONE mask, not two, and the tail is `li r3,1 ; cmplwi ; beqlr` with a
// shared `li r3,0` last -- exactly IsIdle's outer shape, which is `int` and
// needs no normalisation because the two `li`s already produce 0 or 1.  So
// this is the guard form joined with `||`, and NOT the `&&` chain that
// l9/l10 have: there the value flows through r11 into a masked bool return,
// here it is written straight into r3.
//
// The first guard's `bne-` jumps forward to that same shared zero, which is
// what says the failure path is written LAST.

#include "types.h"

struct IdleDeep
{
    /* 0x0000 */ char unk0000[0x8C0];
    /* 0x08C0 */ s32  kind;
    /* 0x08C4 */ char unk08C4[0x14];
    /* 0x08D8 */ s32  b;
    /* 0x08DC */ s32  c;
};
ASSERT_OFFSET(IdleDeep, kind, 0x8C0);
ASSERT_OFFSET(IdleDeep, b,    0x8D8);
ASSERT_OFFSET(IdleDeep, c,    0x8DC);

struct IdleTop
{
    /* 0x00 */ char      unk0000[0x08];
    /* 0x08 */ IdleDeep* deep;
};
ASSERT_OFFSET(IdleTop, deep, 0x08);

static bool IsPending(const IdleDeep* d)
{
    return d->b != 0 || d->c != 0;
}

int IsKind1AndIdle(IdleTop* t)
{
    IdleDeep* d = t->deep;

    if (d->kind != 1 || IsPending(d))
        return 0;
    return 1;
}
