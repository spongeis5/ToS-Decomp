#include "types.h"

// sub_826918F8 -- write 1.0f and 0.0f into six consecutive float slots.
// 44 B, 82 callers.
//   lis r11,-32256 ; lis r10,-32256
//   lfs f13,11584(r11)      -> 82002D40 = 1.0f
//   lfs f0,11684(r10)       -> 82002DA4 = 0.0f
//   stfs f13,0(r3) ; stfs f0,4(r3) ; stfs f0,8(r3)
//   stfs f0,12(r3) ; stfs f13,16(r3) ; stfs f0,20(r3) ; blr
//
// The six slots are (1,0,0) then (0,1,0): two unit basis vectors, not a
// single six-float blob. Store order is address order here, so the source
// order is the obvious one. Each of the two pooled constants gets its own
// `lis`, which is what MSVC does when two distinct pool entries are live at
// once.
struct BVec3 { f32 x; f32 y; f32 z; };
ASSERT_OFFSET(BVec3, x, 0x00);
ASSERT_OFFSET(BVec3, y, 0x04);
ASSERT_OFFSET(BVec3, z, 0x08);

struct Basis2
{
    BVec3 a;
    BVec3 b;
};
ASSERT_OFFSET(Basis2, b, 0x0C);

void ResetBasis(Basis2* p)
{
    p->a.x = 1.0f;
    p->a.y = 0.0f;
    p->a.z = 0.0f;
    p->b.x = 0.0f;
    p->b.y = 1.0f;
    p->b.z = 0.0f;
}
