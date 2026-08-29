// sub_821FF7B8 -- store a float three levels down, if every link is there.
// 44 B, 3 callers.
//
//      lwz    r11,56(r3)  ; cmplwi cr6,r11,0 ; beqlr cr6
//      lwz    r11,76(r11) ; cmplwi cr6,r11,0 ; beqlr cr6
//      lwz    r11,12(r11) ; cmplwi cr6,r11,0 ; beqlr cr6
//      stfs   f1,16(r11)
//
// Three identical guard triples, each ending in a RETURN rather than a
// branch to a shared exit -- `beqlr` is the whole of the failure path, so
// there is nothing to share and the guards are written as early returns
// rather than joined with `&&`.
//
// The chain walks through one register: each load overwrites r11 with the
// next link, so nothing is kept live and no `mr` appears.  That is what a
// local per level gives, each one dead after its own guard.
//
// The float parameter is the second argument and lands in f1 with r4
// unused, which is the ordinary slot assignment.

#include "types.h"

struct FloatTarget
{
    /* 0x00 */ char unk0000[0x10];
    /* 0x10 */ f32  value;
};
ASSERT_OFFSET(FloatTarget, value, 0x10);

struct Leaf
{
    /* 0x00 */ char         unk0000[0x0C];
    /* 0x0C */ FloatTarget* target;
};
ASSERT_OFFSET(Leaf, target, 0x0C);

struct Mid
{
    /* 0x00 */ char  unk0000[0x4C];
    /* 0x4C */ Leaf* leaf;
};
ASSERT_OFFSET(Mid, leaf, 0x4C);

struct Root
{
    /* 0x00 */ char unk0000[0x38];
    /* 0x38 */ Mid* mid;
};
ASSERT_OFFSET(Root, mid, 0x38);

void SetDeepValue(Root* r, float v)
{
    Mid* m = r->mid;
    if (m == 0)
        return;

    Leaf* l = m->leaf;
    if (l == 0)
        return;

    FloatTarget* t = l->target;
    if (t == 0)
        return;

    t->value = v;
}
