#include "types.h"
#include <math.h>

// sub_82161630 -- normalise a Vec3 in place; if it is degenerate, copy a
// fallback vector over it instead. 112 B, 13 callers.
// r3 = the vector (read and written), r4 = the fallback.
//
//   lfs f13,4(r3) ; fmuls f10,f13,f13     y*y first, as in sub_8214D640
//   lfs f0,0(r3) ; lfs f12,8(r3)
//   lfs f11,12012(r11)      -> 82002EEC = 1e-5f
//   fmadds f9,f0,f0,f10 ; fmadds f8,f12,f12,f9     FUSED from a*b+c
//   fsqrts f1,f8
//   fcmpu cr6,f1,f11 ; bge- -> the normalising path
//     lwz/stw x3 : a 12-byte STRUCT COPY, not three float assignments
//   lfs f11,11584(r11)      -> 82002D40 = 1.0f
//   fdivs f11,f11,f1 ; three fmuls/stfs
//
// The three integer load/store pairs are the tell for `*v = *fallback;` --
// a POD struct assignment. Three `v->x = f->x;` float assignments would be
// lfs/stfs pairs.
//
// x, y and z are NOT reloaded in the normalising path: one pointer, distinct
// offsets, so the stores cannot invalidate the other two components. Compare
// sub_8214D640, which reads and writes through DIFFERENT pointers and reloads
// after every store.
//
// The length stays in f1 across both returns, including the copy path where
// it is less than 1e-5f -- it is the return value.

struct V3 { f32 x; f32 y; f32 z; };
ASSERT_OFFSET(V3, y, 0x04);
ASSERT_OFFSET(V3, z, 0x08);

float NormalizeOrDefault(V3* v, const V3* fallback)
{
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);

    if (len < 1e-5f)
    {
        *v = *fallback;
        return len;
    }

    float inv = 1.0f / len;
    v->x = v->x * inv;
    v->y = v->y * inv;
    v->z = v->z * inv;
    return len;
}
