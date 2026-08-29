#include "types.h"

// sub_826919A8 -- transform a 2D point by a 2x3 affine matrix.
// 14 callers.  r3 = the matrix (this), r4 = out, r5 = in.
//
//   lfs f13,0(r3) ; lfs f0,0(r5) ; fmuls f0,f0,f13
//   lfs f13,4(r3) ; lfs f12,4(r5) ; lfs f11,8(r3)
//   fmadds f0,f13,f12,f0 ; fadds f0,f0,f11 ; stfs f0,0(r4)
//   ... the same again for row 1 ...
//
//   out->x = a*in->x + b*in->y + tx
//   out->y = c*in->x + d*in->y + ty
//
// `in` is reloaded after the store to `out` -- two pointers of the same type,
// so the store may alias and MSVC refetches. That is the same tell as
// sub_8214D640, and it says there are no locals holding the input point.
//
// THE MEMBER-FUNCTION LEVER DECIDED THIS ONE. As a free function
// `Xf(const Mtx23* m, Pt2* out, const Pt2* in)` the body is 13 of 19 words:
// every instruction is right and six loads land in the wrong registers,
// because the two multiplicands of each product come out in the opposite
// order. Nine free-function spellings were tried -- reference parameters,
// float arrays for the matrix, for the point, for both, an explicit
// parenthesised grouping of the two products, a column-major layout, a
// third parameter to move the declaration order, and swapping which struct
// is declared first -- all thirteen. Writing it as a member of Mtx23 gives
// 19 of 19. Const and non-const, pointer and reference parameters all match.
//
// Source order of a commutative float operator is NOT readable here: `a*x`
// versus `x*a`, and `a*x + b*y` versus `b*y + a*x`, compile to identical
// bytes. MSVC canonicalises both, and what the canonical order comes out as
// depends on whether the matrix is reached through `this` or a parameter.
//
// The .pdata row records 140 bytes because the linker merged the unwind
// records of this function and its neighbour at 826919F8 -- the same
// transform without the translation -- which are both frameless leaves.
// This body is 76 bytes and match.py reconciles the row.

struct Pt2 { f32 x; f32 y; };
ASSERT_OFFSET(Pt2, y, 0x04);

struct Mtx23
{
    f32 a;  f32 b;  f32 tx;
    f32 c;  f32 d;  f32 ty;

    void TransformPoint(Pt2* out, const Pt2* in) const;
};
ASSERT_OFFSET(Mtx23, tx, 0x08);
ASSERT_OFFSET(Mtx23, c, 0x0C);
ASSERT_OFFSET(Mtx23, d, 0x10);
ASSERT_OFFSET(Mtx23, ty, 0x14);

void Mtx23::TransformPoint(Pt2* out, const Pt2* in) const
{
    out->x = a * in->x + b * in->y + tx;
    out->y = c * in->x + d * in->y + ty;
}
