#include "types.h"
#include <math.h>

// sub_82152358 -- the three row lengths of a 3x4 matrix, written to a Vec3.
// 100 B, 7 callers, 24 float ops.
//
//   lfs    f0,4(r3)            y            <- y FIRST
//   fmuls  f13,f0,f0           y*y
//   lfs    f12,0(r3)           x
//   lfs    f11,8(r3)           z
//   fmadds f10,f12,f12,f13     x*x + y*y    <- FUSED from a*b + c
//   fmadds f9,f11,f11,f10      + z*z
//   fsqrts f8,f9
//   stfs   f8,0(r4)
//   ... the same eight instructions again from 16(r3) into 4(r4)
//   ... and again from 32(r3) into 8(r4)
//   blr
//
// The load order y, x, z with the first product taken on y is exactly what
// sub_8214D998 (d_normalize3.cpp) emits for `x*x + y*y + z*z` -- MSVC
// evaluates the addition left to right, so the FIRST fmadds needs x*x and
// y*y both available and it issues the y multiply while x is still in
// flight. Nothing about the source says y first.
//
// Row stride is 16, so the rows are Vec4s (or the first three rows of a 4x4).
// No epsilon guard and no reciprocal: this is a plain length per row.
//
// The three rows are reloaded rather than kept live, which is the aliasing
// the compiler cannot remove -- `out` and `m` are different pointers, so
// each store may invalidate the next row. Compare d_normalize3.cpp, which
// writes through the SAME pointer it read and therefore never reloads.

struct SRow
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};
ASSERT_OFFSET(SRow, y, 0x04);
ASSERT_OFFSET(SRow, z, 0x08);
ASSERT_SIZE(SRow, 16);

struct SVec3
{
    f32 x;
    f32 y;
    f32 z;
};
ASSERT_OFFSET(SVec3, z, 0x08);

void MtxRowLengths(const SRow* m, SVec3* out)
{
    out->x = sqrtf(m[0].x * m[0].x + m[0].y * m[0].y + m[0].z * m[0].z);
    out->y = sqrtf(m[1].x * m[1].x + m[1].y * m[1].y + m[1].z * m[1].z);
    out->z = sqrtf(m[2].x * m[2].x + m[2].y * m[2].y + m[2].z * m[2].z);
}
