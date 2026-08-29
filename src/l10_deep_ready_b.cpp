// sub_8224BA20 -- sub_8219ED88 again, one field along. 84 B, 4 callers.
//
//      lwz     r11,2264(r3) ; cmpwi ; bne- one
//      lwz     r11,2268(r3) ; cmpwi ; li r11,0 ; beq- done
// one: li      r11,1
// done:clrlwi  r11,r11,24
//      cmplwi  cr6,r11,0 ; bne- zero
//      lwz     r11,112(r3)  ; cmplwi ; beq- zero
//      lwz     r11,16(r11)  ; cmplwi cr6,r11,7
//      li      r11,1 ; beq- end
// zero:li      r11,0
// end: clrlwi  r3,r11,24
//
// Word for word the same code as sub_8219ED88 (src/l9_deep_ready_a.cpp) with
// the pending pair moved from 0x8D4/0x8D8 to 0x8D8/0x8DC -- so there are at
// least three consecutive s32 flags there, and different callers watch
// different adjacent pairs.  sub_822598E0 (src/l11_kind1_not_pending.cpp)
// watches this same 0x8D8/0x8DC pair, which is what makes the third field
// real rather than an off-by-one reading of the first two.
//
// Two masked bools -- one inlined helper, one bool return -- and one shared
// `li r11,0` reached by all three false exits, so the outer join is an `&&`
// chain.  See l9 for the full reading.

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
    /* 0x0074 */ char         unk0074[0x864];
    /* 0x08D8 */ s32          b;
    /* 0x08DC */ s32          c;
};
ASSERT_OFFSET(ReadyDeep, holder, 0x70);
ASSERT_OFFSET(ReadyDeep, b,      0x8D8);
ASSERT_OFFSET(ReadyDeep, c,      0x8DC);

static bool IsPending(const ReadyDeep* d)
{
    return d->b != 0 || d->c != 0;
}

bool IsReadyKind7(ReadyDeep* d)
{
    return !IsPending(d) && d->holder != 0 && d->holder->kind == 7;
}
