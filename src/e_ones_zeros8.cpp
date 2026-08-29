#include "types.h"

// sub_8277E170 -- write 1.0f into four fields and 0.0f into the four that
// interleave them. 52 B, 20 callers.
//
//   lis r11,-32256 ; lis r10,-32256
//   lfs f0,11584(r11)   -> 82002D40 = 1.0f
//   lfs f13,11684(r10)  -> 82002DA4 = 0.0f
//   stfs f0,0(r3) ; stfs f0,8(r3) ; stfs f0,16(r3) ; stfs f0,24(r3)
//   stfs f13,4(r3) ; stfs f13,12(r3) ; stfs f13,20(r3) ; stfs f13,28(r3)
//
// Eight floats at 0..28. The stores are grouped by VALUE, not by address:
// all four 1.0f first, then all four 0.0f. Store order is source order, so
// the source writes the four ones first.

struct OnesZeros8
{
    f32 a0; f32 b0;
    f32 a1; f32 b1;
    f32 a2; f32 b2;
    f32 a3; f32 b3;
};
ASSERT_OFFSET(OnesZeros8, a2, 0x10);
ASSERT_OFFSET(OnesZeros8, b3, 0x1C);

void InitOnesZeros8(OnesZeros8* p)
{
    p->a0 = 1.0f;
    p->a1 = 1.0f;
    p->a2 = 1.0f;
    p->a3 = 1.0f;
    p->b0 = 0.0f;
    p->b1 = 0.0f;
    p->b2 = 0.0f;
    p->b3 = 0.0f;
}
