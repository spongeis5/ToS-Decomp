// sub_8252D9C8 -- shift three arguments up one slot, prepend a field of a
// global object and append a zero, then tail call. 32 bytes, 33 callers.
//
//      lis     r11,-32105
//      mr      r6,r5
//      mr      r5,r4
//      mr      r4,r3
//      li      r7,0
//      lwz     r11,-14932(r11)     g_context   (0x8296CBAC)
//      lwz     r3,4(r11)
//      b       0x8252D4D0
//
// The single `lis` feeding a `lwz` with a signed displacement is a global
// POINTER variable read, not the address of a global object -- the latter
// forms the address with lis+addi first (compare src/global_field.cpp).
//
// The three `mr`s run before the global is touched because computing the new
// first argument clobbers r3; the shift has to happen first. `mr` rather than
// `rlwinm ...,0,0,31` says the arguments are signed or pointer-typed.

#include "types.h"

struct Target;

struct Context
{
    /* 0x00 */ char    unk0000[0x04];
    /* 0x04 */ Target* target;
};

ASSERT_OFFSET(Context, target, 0x04);

extern Context* g_context;

int Dispatch(Target* t, int a, int b, int c, int d);

int Forward(int a, int b, int c)
{
    return Dispatch(g_context->target, a, b, c, 0);
}
