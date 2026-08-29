// sub_82155580 -- quaternion product, then TAIL CALL to the normaliser at
// 82155080 (src/e_normalize4.cpp, `QuatNormalize(out, in)`) with the product
// as both arguments. 124 B, 6 callers.  r3 = out, r4 = a, r5 = b.
//
//   mr r11,r4 ; mr r4,r3        a moves out of r4 so r4 can carry `out`
//   ... 8 lfs, 4 fmuls, 8 fmadds/fmsubs, 4 fnmsubs, 4 stfs ...
//   b 0x82155080
//
// THE FOUR RESULTS HAVE TO BE LOCALS. `out` may alias `a` or `b`, so
// assigning straight through the pointer makes MSVC finish one component,
// store it, and RELOAD every input before the next -- 2 of 31 words, with
// the loads scattered through the arithmetic. Computing four floats first
// and storing them afterwards is what puts all eight `lfs` in front.
//
// Every component of both inputs is loaded before any store, and the four
// dependency chains are interleaved one instruction apiece in the order
// x, w, y, z -- at the seeds, at both fma stages and at the stores (0, 12,
// 4, 8). That order is the source order; nothing in the arithmetic makes
// w second.
//
// Reading each chain back gives the term order too. `((p+q)+r)-s` with all
// four terms products compiles as fmuls(q), fmadds(p,.), fmadds(r,.),
// fnmsubs(s,.) -- the fmuls is the RIGHT operand of the first `+`, which is
// how the operand order of `fmsubs f1,f0,f5,f9` (w) reads out as
// `a.w*b.w - a.x*b.x` and not the reverse. So:
//
//   x   fmuls a.x*b.w ; fmadds a.y*b.z ; fmadds a.w*b.x ; fnmsubs a.z*b.y
//   w   fmuls a.x*b.x ; fmsubs a.w*b.w ; fnmsubs a.y*b.y ; fnmsubs a.z*b.z
//   y   fmuls a.y*b.w ; fmadds a.z*b.x ; fmadds a.w*b.y ; fnmsubs a.x*b.z
//   z   fmuls a.x*b.y ; fmadds a.z*b.w ; fmadds a.w*b.z ; fnmsubs a.y*b.x
//
// which is the standard product with its first three terms written in the
// order (cross, scale-by-a.w's partner, scale-by-b's) rather than the usual
// w-first spelling. x and y put the same term in the fmuls slot; z swaps its
// first two, which is the commutative choice the scheduler is free to make
// (MATCHED.md: float operand order under /fp:fast is not readable).

#include "types.h"

struct Quat { f32 x; f32 y; f32 z; f32 w; };

ASSERT_OFFSET(Quat, z, 0x08);
ASSERT_OFFSET(Quat, w, 0x0C);

float QuatNormalize(Quat* out, const Quat* in);

float QuatMulNormalize(Quat* out, const Quat* a, const Quat* b)
{
    float x = a->y * b->z + a->x * b->w + a->w * b->x - a->z * b->y;
    float w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    float y = a->z * b->x + a->y * b->w + a->w * b->y - a->x * b->z;
    float z = a->x * b->y + a->z * b->w + a->w * b->z - a->y * b->x;

    out->x = x;
    out->w = w;
    out->y = y;
    out->z = z;
    return QuatNormalize(out, out);
}
