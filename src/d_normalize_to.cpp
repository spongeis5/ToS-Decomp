#include "types.h"
#include <math.h>

// sub_8214D640 -- normalise one Vec3 into another and report its length.
// 176 B, 35 callers.  r3 = float* out-length, r4 = destination, r5 = source.
//
//   lfs f0,4(r5) ; fmuls f13,f0,f0
//   lfs f0,0(r5) ; lfs f10,8(r5)
//   lfs f12,11584(r11)      -> 82002D40 = 1.0f
//   lfs f11,12012(r10)      -> 82002EEC = 1e-5f
//   fmadds f9,f0,f0,f13     x*x + y*y      (FUSED from a*b+c)
//   fmadds f13,f10,f10,f9   + z*z          -> len2
//   fsubs  f8,f13,f12       len2 - 1.0f
//   fabs   f7,f8
//   fcmpu  cr6,f7,f11 ; bgt- cr6,0x8214d694
//     -- fall-through: already unit, copy through and report 1.0f
//   fabs f0,f13 ; fcmpu cr6,f0,f11 ; bgt- cr6,0x8214d6bc
//     -- fall-through: degenerate, emit (0,1,0) and report 0.0f
//   fsqrts f0,f13 ; stfs f0,0(r3)
//   fdivs f12,f12,f0        1.0f / len, reusing the SAME pooled 1.0f
//   three fmuls/stfs pairs, each reloading its component
//
// Both guards are `bgt-` jumping AWAY, so both fall-through cases are the
// interesting ones and are written first, innermost last.
//
// The reloads are the tell that this is plain member access with NO locals:
// `out` and `in` are different pointers of the same type, so every store
// through `out` invalidates what the compiler knew about `in`, and it fetches
// in->y and in->z again after writing out->x. `in->x` survives only because
// it was already loaded before the first store. Copying the source into
// locals to "explain" the single load would be exactly wrong here -- and is
// also wrong in sub_8214D998, for the mirror-image reason.
//
// The 1.0f the guard compares against and the 1.0f the reciprocal divides are
// the same pool entry and the same register, f12, held live across the whole
// body.
//
// The store order in the degenerate arm is y, x, z, len rather than source
// order: MSVC hoists the 1.0f store because f12 is already loaded while 0.0f
// still needs its lis/lfs. Both source orders compile to the same 44 words.

struct MVec3 { f32 x; f32 y; f32 z; };
ASSERT_OFFSET(MVec3, x, 0x00);
ASSERT_OFFSET(MVec3, y, 0x04);
ASSERT_OFFSET(MVec3, z, 0x08);

void NormalizeTo(float* outLen, MVec3* out, const MVec3* in)
{
    float len2 = in->x * in->x + in->y * in->y + in->z * in->z;

    if (fabsf(len2 - 1.0f) <= 1e-5f)
    {
        out->x = in->x;
        out->y = in->y;
        out->z = in->z;
        *outLen = 1.0f;
        return;
    }

    if (fabsf(len2) <= 1e-5f)
    {
        out->x = 0.0f;
        out->y = 1.0f;
        out->z = 0.0f;
        *outLen = 0.0f;
        return;
    }

    float len = sqrtf(len2);
    *outLen = len;
    float inv = 1.0f / len;
    out->x = in->x * inv;
    out->y = in->y * inv;
    out->z = in->z * inv;
}
