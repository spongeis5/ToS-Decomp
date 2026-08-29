#include "types.h"

// sub_8252D950 -- the six-argument sibling of src/b_fwd_global5.cpp
// (sub_8252D9C8), 0x78 bytes earlier and reading the same global pointer.
// 36 B, 13 callers.
//
//      lis     r11,-32105
//      mr      r6,r5
//      mr      r5,r4
//      mr      r4,r3
//      li      r8,0             <-- the only difference from sub_8252D978
//      lwz     r11,-14932(r11)  g_context
//      li      r7,0
//      lwz     r3,4(r11)
//      b       0x8252D208
//
// lis feeding a lwz with a signed displacement is a read OF a global pointer
// variable, not the address of a global object.

struct Target;

struct Context
{
    /* 0x00 */ char    unk0000[0x04];
    /* 0x04 */ Target* target;
};

ASSERT_OFFSET(Context, target, 0x04);

extern Context* g_context;

int Dispatch6(Target* t, int a, int b, int c, int d, int e);

int Forward6A(int a, int b, int c)
{
    return Dispatch6(g_context->target, a, b, c, 0, 0);
}
