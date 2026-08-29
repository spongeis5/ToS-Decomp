#include "types.h"
#include <math.h>

// sub_8214D998 -- normalise a Vec3 in place, return its length. 96 B, 50 callers.
//
//   lfs f13,4(r3) ; fmuls f11,f13,f13
//   lfs f0,0(r3)  ; lfs f12,8(r3)
//   lfs f10,12036(r11)              -> 82002F04 = 1e-10f
//   fmadds f9,f0,f0,f11             x*x + y*y   -- FUSED from plain a*b+c
//   fmadds f11,f12,f12,f9           + z*z
//   fcmpu cr6,f11,f10 ; bge- cr6,0x8214d9cc
//   lfs f1,11684(r11)               -> 82002DA4 = 0.0f
//   blr                             fall-through: len2 < 1e-10 -> 0.0f
// 0x8214d9cc:
//   fsqrts f1,f11                   f1 is the RETURN VALUE and survives
//   lfs f11,11584(r11)              -> 82002D40 = 1.0f
//   fdivs f11,f11,f1                1.0f / len, written literally
//   fmuls f10,f0,f11 ; stfs f10,0(r3)
//   fmuls f9,f13,f11 ; stfs f9,4(r3)
//   fmuls f8,f12,f11 ; stfs f8,8(r3)
//   blr
//
// `bge-` skips the zero return, so the epsilon guard is the fall-through and
// is written first.
//
// THE ONE THING THAT DECIDED THIS FUNCTION: write the members directly, not
// through locals. The three components are loaded once and never reloaded
// across the three stores, and the instinct is that the source must have
// copied them into locals to make that safe. It did not -- v->x and v->y are
// different offsets off the SAME pointer, so MSVC already knows they cannot
// alias and keeps them live by itself.
//
// Copying into locals first compiles to the same 18 words except that every
// `fmuls` comes out with its two operands in the opposite slots
// (`fmuls f10,f11,f0` for the target's `fmuls f10,f0,f11`), and no ordering
// of the declarations, of the sum, or of the multiply's operands moves it --
// all twelve orderings of the local form give the identical three-word miss.
// Members: 0 differ. Compare sub_8214D640, which DOES reload, because there
// the destination is a different pointer from the source and the stores can
// alias.

struct NVec3 { f32 x; f32 y; f32 z; };
ASSERT_OFFSET(NVec3, x, 0x00);
ASSERT_OFFSET(NVec3, y, 0x04);
ASSERT_OFFSET(NVec3, z, 0x08);

float Normalize3(NVec3* v)
{
    float len2 = v->x * v->x + v->y * v->y + v->z * v->z;

    if (len2 < 1e-10f)
        return 0.0f;

    float len = sqrtf(len2);
    float inv = 1.0f / len;
    v->x *= inv;
    v->y *= inv;
    v->z *= inv;
    return len;
}
