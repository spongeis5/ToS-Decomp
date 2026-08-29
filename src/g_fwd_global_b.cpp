#include "types.h"

// sub_8252D978 -- identical to sub_8252D950 apart from the last argument,
// which is 1 rather than 0. 36 B, 13 callers. The two sit 0x28 apart, so they
// are the same translation unit and must want the same flags.
//
//      lis     r11,-32105
//      mr      r6,r5
//      mr      r5,r4
//      mr      r4,r3
//      li      r8,1
//      lwz     r11,-14932(r11)  g_context
//      li      r7,0
//      lwz     r3,4(r11)
//      b       0x8252D208

struct Target;

struct Context
{
    /* 0x00 */ char    unk0000[0x04];
    /* 0x04 */ Target* target;
};

ASSERT_OFFSET(Context, target, 0x04);

extern Context* g_context;

int Dispatch6(Target* t, int a, int b, int c, int d, int e);

int Forward6B(int a, int b, int c)
{
    return Dispatch6(g_context->target, a, b, c, 0, 1);
}
