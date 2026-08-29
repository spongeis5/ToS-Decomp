// sub_822AE7C0 -- write three fields of a small record, then tail-call a
// notifier with the SAME float constant. 32 bytes, 4 callers.
//
//      lis     r11,-32256
//      stb     r5,0(r3)
//      li      r10,0
//      stb     r10,1(r3)
//      lfs     f1,11584(r11)      -> 82002D40, which holds 1.0f
//      stfs    f1,4(r3)
//      lwz     r3,56(r4)
//      b       0x82237870
//
// The constant is READ ONCE and used twice -- stored into the record at +4
// and left in f1, which is the first float argument register. sub_82237870
// really does take a float: it does `fmr f31,f1` on entry and compares f1
// against 82002DA4 (0.0f) and against 82002D40 (1.0f), the very word loaded
// here. So the source writes the field and passes the same literal, and MSVC
// common-subexpressions the `lfs`.
//
// r3 is reloaded from the SECOND parameter's +56 for the call, so the record
// pointer is dead after the three stores -- this is a void helper, not a
// function returning the record.
//
// Stores come out at 0, 1, 4 -- source order, as usual for a store group.
//
// The lis and the tail branch are relocated, so 6 of 8 words are compared.

#include "types.h"

struct CueRec
{
    /* 0x00 */ u8  kind;
    /* 0x01 */ u8  busy;
    /* 0x02 */ u8  unk0002[2];
    /* 0x04 */ f32 amount;
};

ASSERT_OFFSET(CueRec, busy, 0x01);
ASSERT_OFFSET(CueRec, amount, 0x04);

struct Blender;

struct CueOwner
{
    /* 0x00 */ char     unk0000[0x38];
    /* 0x38 */ Blender* blender;
};

ASSERT_OFFSET(CueOwner, blender, 0x38);

void SetBlend(Blender* b, float amount);

void BeginCue(CueRec* r, CueOwner* o, u8 kind)
{
    r->kind = kind;
    r->busy = 0;
    r->amount = 1.0f;
    SetBlend(o->blender, 1.0f);
}
