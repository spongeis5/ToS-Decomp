// sub_825E35E0 -- virtual call slot 6 on a global singleton, 0 when it is
// null. 40 B, 5 callers.
//
//      lis     r11,-32093
//      lwz     r3,18444(r11)      g_singleton  (0x82A3480C)
//      cmplwi  cr6,r3,0
//      beq-    cr6,0x825E3600
//      lwz     r11,0(r3)
//      lwz     r11,24(r11)        slot 24/4 = 6
//      mtctr   r11
//      bctr
//  825E3600:
//      li      r3,0
//      blr
//
// A single `lis` feeding a `lwz` with a displacement is a global POINTER
// variable read, not the address of a global object -- the same read as
// src/vcall_global_2.cpp (slot 2) and src/vcall_global_4.cpp (slot 4), which
// are 72 and 24 bytes earlier and take the same word at 0x82A3480C. Those two
// are both `/O2 /Os`, and a translation unit is contiguous, so this one is
// tried at that level first.
//
// The loaded pointer goes STRAIGHT into r3 with no `mr`/`rotlwi` copy, which
// is the named-local spelling (a_vcall4_or_neg1.cpp is the control: spelling
// the expression out at each use makes MSVC CSE it into a scratch and copy it
// back into r3).
//
// `beq-` jumps AWAY to the zero return, so the call is the fall-through and
// is written FIRST.

#include "types.h"

struct GT6;

struct GVT6
{
    void* (*slot[7])(GT6*);
};

struct GT6
{
    /* 0x00 */ GVT6* vt;
};

ASSERT_OFFSET(GT6, vt, 0x00);

extern GT6* g_singleton_825E35E0;

void* CallGlobal6()
{
    GT6* g = g_singleton_825E35E0;
    if (g)
        return g->vt->slot[6](g);
    return 0;
}
