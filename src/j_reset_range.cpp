#include "types.h"

// sub_82799B98 -- reset a block of fields to their defaults. 76 B, 13 callers.
// /O2 /Os.
//
//   82002D40 = 1.0f  -> 0x34, 0x38
//   82002DA4 = 0.0f  -> 0x3C, 0x40
//   0 -> 0x08, 0x18, 0x28 ; 0x7FFFFFFF -> 0x2C ; 0x80000001 -> 0x30
//
// The stores come out interleaved one integer, one float, but each stream is
// internally in ADDRESS order, which is the source order: the alternation is
// the scheduler filling both pipes, not the source alternating. Writing all
// five integer stores and then all four float stores is what matches.
//
// /Os DECIDED THIS ONE, and by the documented signature. At /O2 the two large
// constants are built into fresh registers -- `ori r6,r8,65535` and
// `ori r5,r7,1`, then stored from r6 and r5 -- where the target writes back
// into r8 and reuses r10, the register its own (now dead) `lis` for the float
// pool left behind. Same instructions, same order but for one swapped pair,
// four words wrong. 15 of 15 at /O2 /Os with no change to the source.

struct ResetRange
{
    u8  pad00[0x08];
    s32 a08;
    u8  pad0C[0x0C];
    s32 a18;
    u8  pad1C[0x0C];
    s32 a28;
    s32 a2C;
    s32 a30;
    f32 a34;
    f32 a38;
    f32 a3C;
    f32 a40;
};
ASSERT_OFFSET(ResetRange, a08, 0x08);
ASSERT_OFFSET(ResetRange, a18, 0x18);
ASSERT_OFFSET(ResetRange, a28, 0x28);
ASSERT_OFFSET(ResetRange, a2C, 0x2C);
ASSERT_OFFSET(ResetRange, a30, 0x30);
ASSERT_OFFSET(ResetRange, a34, 0x34);
ASSERT_OFFSET(ResetRange, a40, 0x40);

void Reset(ResetRange* p)
{
    p->a08 = 0;
    p->a18 = 0;
    p->a28 = 0;
    p->a2C = 0x7FFFFFFF;
    p->a30 = (s32)0x80000001;
    p->a34 = 1.0f;
    p->a38 = 1.0f;
    p->a3C = 0.0f;
    p->a40 = 0.0f;
}
