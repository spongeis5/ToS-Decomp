// sub_8252D9A0 -- the four-argument sibling of src/b_fwd_global5.cpp
// (sub_8252D9C8) and src/g_fwd_global_a.cpp / _b.cpp (8252D950, 8252D978):
// shift the arguments up one slot, prepend a field of the same global
// object, append a zero, tail call. 36 bytes, 5 callers.
//
//      lis     r11,-32105
//      mr      r7,r6
//      mr      r6,r5
//      mr      r5,r4
//      mr      r4,r3
//      lwz     r11,-14932(r11)     g_context   (0x8296CBAC)
//      li      r8,0
//      lwz     r3,4(r11)
//      b       0x8252D618
//
// Same global at the same offset as its three neighbours, read the same way:
// a single lis feeding a lwz with a signed displacement is a global POINTER
// variable, not the address of a global object.
//
// The four `mr`s run before the global is touched because building the new
// first argument clobbers r3, so the shift has to happen first. `li r8,0`
// lands after the pointer load here rather than before it as in the
// three-argument siblings -- one more `mr` in front of it is the only
// difference.
//
// Two words are relocations (the lis and the tail branch), so 7 of 9 are
// compared.

#include "types.h"

struct Target;

struct Context
{
    /* 0x00 */ char    unk0000[0x04];
    /* 0x04 */ Target* target;
};

ASSERT_OFFSET(Context, target, 0x04);

extern Context* g_context;

int Dispatch6b(Target* t, int a, int b, int c, int d, int e);

int Forward4(int a, int b, int c, int d)
{
    return Dispatch6b(g_context->target, a, b, c, d, 0);
}
