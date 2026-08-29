#include "types.h"

// sub_8214CCB8 -- reset a block of fields to their defaults. 84 B, 8 callers.
//
//   82002D40 = 1.0f     -> 0x12C
//   82002DA4 = 0.0f     -> 0x130, 0x134, 0x13C
//   82002E6C = 10000.0f -> 0x138
//   0  -> 0x94, 0x98, 0xA8, 0x128 (byte), 0x08 (std, 64 bits)
//   -1 -> 0xB8, 0xBC
//
// The integer stores come out 0x94, 0xB8, 0xBC, 0xA8, 0x98, 0x128, 0x08 --
// NOT address order, so that is source order. The float stores come out
// ascending and interleaved one-for-one between them, which is the scheduler
// filling both pipes, not the source alternating.

struct ResetState
{
    u8  pad00[0x08];
    s64 a08;
    u8  pad10[0x84];
    s32 a94;
    s32 a98;
    u8  pad9C[0x0C];
    s32 aA8;
    u8  padAC[0x0C];
    s32 aB8;
    s32 aBC;
    u8  padC0[0x68];
    u8  a128;
    u8  pad129[0x03];
    f32 a12C;
    f32 a130;
    f32 a134;
    f32 a138;
    f32 a13C;
};
ASSERT_OFFSET(ResetState, a08, 0x08);
ASSERT_OFFSET(ResetState, a94, 0x94);
ASSERT_OFFSET(ResetState, a98, 0x98);
ASSERT_OFFSET(ResetState, aA8, 0xA8);
ASSERT_OFFSET(ResetState, aB8, 0xB8);
ASSERT_OFFSET(ResetState, aBC, 0xBC);
ASSERT_OFFSET(ResetState, a128, 0x128);
ASSERT_OFFSET(ResetState, a12C, 0x12C);
ASSERT_OFFSET(ResetState, a13C, 0x13C);

void ResetAll(ResetState* p)
{
    p->a94 = 0;
    p->aB8 = -1;
    p->aBC = -1;
    p->aA8 = 0;
    p->a98 = 0;
    p->a128 = 0;
    p->a08 = 0;

    p->a12C = 1.0f;
    p->a130 = 0.0f;
    p->a134 = 0.0f;
    p->a138 = 10000.0f;
    p->a13C = 0.0f;
}
