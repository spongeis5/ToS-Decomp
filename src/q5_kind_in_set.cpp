#include "types.h"

// sub_821F5EE0 -- two inlined bool predicates over the same `kind` field,
// joined by `||`. 88 B, 8 callers.
//
//      lwz     r11,2240(r3)          k = d->kind        (0x8C0)
//      cmpwi   cr6,r11,2 ; beq- yes1
//      cmpwi   cr6,r11,3 ; beq- yes1
//      cmpwi   cr6,r11,4 ; beq- yes1
//      cmpwi   cr6,r11,5
//      li      r10,0
//      bne-    cr6,done1
// yes1:li      r10,1
// done1:clrlwi r10,r10,24            <- a bool, materialised then masked
//      cmplwi  cr6,r10,0
//      bne-    cr6,yes2
//      cmpwi   cr6,r11,1 ; beq- yes2
//      cmpwi   cr6,r11,6
//      li      r11,0
//      bne-    cr6,done2
// yes2:li      r11,1
// done2:clrlwi r3,r11,24
//      blr
//
// Exactly the sub_8219FCD8 shape (i_state_idle.cpp) one level simpler: two
// `||` chains each materialised into 0/1 and masked to 8 bits -- the
// signature of an inlined bool-returning helper, not a bare `if`.  Here the
// OUTER join is also a `||` and its value IS the return value, so there is no
// `li r3,1 / cmplwi / beqlr` tail: the second chain's bool is simply masked
// into r3.
//
// cmpwi throughout, so `kind` is signed.  The case order 2,3,4,5 then 1,6 is
// source order; `||` does not get reordered.
//
// 0x8C0 is the same `kind` offset i_state_idle.cpp established, but reached
// directly off the argument here rather than through a pointer at +8.
struct KindObj
{
    /* 0x0000 */ char unk0000[0x8C0];
    /* 0x08C0 */ s32  kind;
};
ASSERT_OFFSET(KindObj, kind, 0x8C0);

static bool IsMoving(const KindObj* d)
{
    return d->kind == 2 || d->kind == 3 || d->kind == 4 || d->kind == 5;
}

static bool IsHeld(const KindObj* d)
{
    return d->kind == 1 || d->kind == 6;
}

bool IsActiveKind(KindObj* d)
{
    if (IsMoving(d))
        return true;
    return IsHeld(d);
}
