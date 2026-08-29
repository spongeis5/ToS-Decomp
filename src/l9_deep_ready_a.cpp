// sub_8219ED88 -- "nothing pending, and the holder is kind 7". 84 B, 4 callers.
//
//      lwz     r11,2260(r3)
//      cmpwi   cr6,r11,0
//      bne-    cr6,one
//      lwz     r11,2264(r3)
//      cmpwi   cr6,r11,0
//      li      r11,0
//      beq-    cr6,done
// one: li      r11,1
// done:clrlwi  r11,r11,24        <- mask 1: an inlined bool helper
//      cmplwi  cr6,r11,0
//      bne-    cr6,zero
//      lwz     r11,112(r3)
//      cmplwi  cr6,r11,0
//      beq-    cr6,zero
//      lwz     r11,16(r11)
//      cmplwi  cr6,r11,7
//      li      r11,1
//      beq-    cr6,end
// zero:li      r11,0
// end: clrlwi  r3,r11,24         <- mask 2: the bool return
//      blr
//
// COUNT THE MASKED BOOLS: two, so ONE inlined helper plus a bool return.
// The helper is the `d->a != 0 || d->b != 0` of src/i_state_idle.cpp, on the
// same 0x8D4/0x8D8 pair -- there reached through a pointer at +8, here on the
// object itself.
//
// The outer join is an `&&` CHAIN, not a sequence of guards: all three false
// exits reach ONE `li r11,0` planted immediately before the return's mask,
// and the last term materialises 1 and falls into it.  Flat `if (...) return
// false;` guards would each get their own zero (the twelve-word failure
// recorded on sub_8219FCD8).
//
// Signedness is readable and is not uniform: cmpwi on 0x8D4/0x8D8 (signed,
// as i_state_idle already established), cmplwi on the pointer at 0x70, and
// cmplwi against 7 -- so the field at +0x10 of the holder is UNSIGNED.  A
// signed int there would compare with cmpwi.

#include "types.h"

struct ReadyHolder
{
    /* 0x00 */ char unk0000[0x10];
    /* 0x10 */ u32  kind;
};
ASSERT_OFFSET(ReadyHolder, kind, 0x10);

struct ReadyDeep
{
    /* 0x0000 */ char         unk0000[0x70];
    /* 0x0070 */ ReadyHolder* holder;
    /* 0x0074 */ char         unk0074[0x860];
    /* 0x08D4 */ s32          a;
    /* 0x08D8 */ s32          b;
};
ASSERT_OFFSET(ReadyDeep, holder, 0x70);
ASSERT_OFFSET(ReadyDeep, a,      0x8D4);
ASSERT_OFFSET(ReadyDeep, b,      0x8D8);

static bool IsPending(const ReadyDeep* d)
{
    return d->a != 0 || d->b != 0;
}

bool IsReadyKind7(ReadyDeep* d)
{
    return !IsPending(d) && d->holder != 0 && d->holder->kind == 7;
}
