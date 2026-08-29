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
// variable read, not the address of a global object. It is the same word
// src/vcall_global_2.cpp (slot 2) and src/vcall_global_4.cpp (slot 4) read,
// 72 and 24 bytes earlier; both of those need /O2 /Os and a translation unit
// is contiguous, so this one is one too.
//
// NEEDS /O2 /Os, and it took BOTH that and a source shape, because the two
// levels fail in different places:
//
//      /O2   cr6 right, and the slot lands in a FRESH r10  (6 of 8)
//      /Os   slot reuses r11, and the compare moves to cr0 (6 of 8)
//
// All 72 flag combinations tools/flagsweep.py sweeps score 6 of 8
// non-relocated words -- the two failures are simply distributed differently
// across the same bucket -- so the flag axis alone cannot reach it.
//
// What closes it is UN-NAMING the singleton. `GT6* g = g_singleton;` then
// three uses of `g` compiles the compare into cr0 at /Os; spelling
// `g_singleton` out at all three uses gives cr6 and 8 of 8. Sixteen shapes
// were measured at /Os and exactly two reach it -- this one and the same
// un-naming applied to a real `virtual` member call, which is the same
// change. Return type (void*/int/u32), ternary versus if, if/else, a named
// vtable member, an inlined accessor, a const local and a local function
// pointer are all 6 of 8; the inverted `if (g == 0) return 0;` polarity is
// 0 of 7 and a word short, which is the polarity lever confirming itself.
//
// That is the mirror image of src/a_vcall4_or_neg1.cpp, where repeating the
// expression was what produced a redundant-looking `mr`. Here there is no
// `mr` to produce -- the global read already lands in r3 -- and what the
// repetition moves is the CONDITION REGISTER FIELD. Same lever, different
// symptom: naming the value tells MSVC it lives in one place for the whole
// function, and that decision reaches further than the register allocator.
//
// `beq-` jumps AWAY to the zero return, so the call is the fall-through and
// is written FIRST.
//
// 2 of 10 words are relocated (the `lis`/`lwz` pair), so 8 are compared.

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
    if (g_singleton_825E35E0)
        return g_singleton_825E35E0->vt->slot[6](g_singleton_825E35E0);
    return 0;
}
