#include "types.h"
#include <math.h>

// sub_82155080 -- normalise a quaternion from `in` into `out` and return the
// length it had. 204 B, 16 callers.  r3 = out, r4 = in.  /O2.
//
// NOT A MATCH: 45 of 51 words at /O2 (4 more differ in relocated words, which
// is expected). What is left is TWO `fmuls` whose two multiplicands are in the
// opposite A/C slots; see the bottom of this comment, which is the useful part.
//
//   len2 = x*x + y*y + z*z + w*w        (three fmadds, FUSED from a*b+c)
//   lfs f1,11584(r11)      -> 82002D40 = 1.0f, loaded into the RETURN
//                             register before the first compare
//   fcmpu cr6,f13,f1 ; bne- -> already unit: copy through, return 1.0f
//   lfs f0,11684(r11)      -> 82002DA4 = 0.0f
//   fcmpu cr6,f13,f0 ; bne- -> degenerate: emit (0,0,0,1), return 0.0f
//   fsqrts f0,f13 ; fdivs f12,f1,f0     1.0f / len, reusing the SAME pooled
//                                       1.0f the guard compared against
//   four fmuls/stfs pairs ; fmr f1,f0 ; blr
//
// Read straight off the disassembly, and all of it reproduces:
//
//   * `cmplw cr6,r3,r4` guards each copy path -- the source tests the two
//     POINTERS and skips the writes when they are the same object. In the
//     degenerate path that test `beq-`s to the shared `fmr f1,f0 ; blr` at
//     82155144 rather than to its own return, because f0 holds 0.0f there and
//     holds `len` on the fall-through: MSVC tail-merged `return 0.0f` with
//     `return len`.
//
//   * w is stored FIRST in all three arms -- 12, then 0, 4, 8. All 24
//     orderings of the four scaling statements were compiled; w,x,y,z is the
//     only one that puts the stores where the target has them, and it is also
//     the best-scoring (the next best, y,x,w,z, is 43/51).
//
//   * The equality tests are `==`, not an epsilon: `fcmpu` + `bne-` with no
//     fsubs/fabs in front. Compare sub_8214D640, which does use an epsilon
//     and spends four extra instructions on it.
//
// WHAT IS LEFT, and what it is not:
//
//     82155118  want fmuls f11,f13,f12    (in->w, inv)   got (inv, in->w)
//     82155130  want fmuls f7,f12,f8      (inv, in->y)   got (in->y, inv)
//
// Every load, every store, every branch and every other opcode is identical.
// The target's four scaling multiplies are (in->w,inv) (in->x,inv)
// (inv,in->y) (inv,in->z); this source gives (inv,in->w) (in->x,inv)
// (in->y,inv) (inv,in->z).
//
// Source operand order does NOT reach this. All 16 combinations of writing
// the four multiplies as `in->c * inv` or `inv * in->c` compile to BYTE
// IDENTICAL code -- MSVC canonicalises commutative float operators under
// /fp:fast. Nor do flags: a 72-combination sweep gives 45/51 for every
// /fp:fast combination and 0/51 for every /fp:precise one, and /Ou, /Oy,
// /Ob2, /Og, /Oi, /Ot and /Ox change nothing.
//
// What DOES move it is how the value being scaled is PRODUCED, the same lever
// that decided sub_82691C50 in this batch. Staging each component through an
// array element first --
//
//     float s[4];
//     s[3] = in->w;  out->w = s[3] * inv;
//     s[0] = in->x;  out->x = s[0] * inv;   ...
//
// -- flips the w multiply into the target's slots and reaches 46 of 51. The
// y multiply then stays wrong under all 625 combinations of spelling the four
// components as a direct member read, an array temp, a scalar temp, a scalar
// result temp or an array result temp, and under a second copy of `inv`, an
// `inv` staged through an array, `1.0f/len` written out at the last two uses,
// and a struct declared as `f32 v[4]`. So the open question is one operand
// slot on one multiply, and it is a codegen choice this file cannot reach
// from the shape of the arithmetic.
//
// STILL 45 of 47 non-relocated words. Eight more shapes, none better than the
// 46 of 47 the array staging already gives:
//
//   * FOUR DIVISIONS instead of a reciprocal multiply. MSVC /fp:fast really
//     does fold them to one `fdivs f12,f1,f0` and four `fmuls` -- the same
//     nineteen instructions -- so the division spelling is NOT
//     distinguishable from the reciprocal one here. It moves a DIFFERENT
//     multiply: x flips to (inv, x) and w stays wrong, 44 of 47. Worth
//     knowing that the transformation happens at all.
//   * a second named copy of `inv` used for the last two components
//   * `1.0f / len` written out again at the last two components
//   * results staged through a float[4] instead of the components
//   * a const view of the input for the last two components
//   * float-array views of both quaternions, `d[i] = s[i] * inv`
//   * array staging for w and x only -- 46 of 47, the same as staging all
//     four, so the staging that fixes w is not doing anything for y or z
//
// Nothing addressed at the ARITHMETIC moves the y multiply; what moved w was
// staging, and staging y the same way does not move y.

struct Quat { f32 x; f32 y; f32 z; f32 w; };
ASSERT_OFFSET(Quat, z, 0x08);
ASSERT_OFFSET(Quat, w, 0x0C);

float QuatNormalize(Quat* out, const Quat* in)
{
    float len2 = in->x * in->x + in->y * in->y + in->z * in->z + in->w * in->w;

    if (len2 == 1.0f)
    {
        if (out != in)
        {
            out->w = in->w;
            out->x = in->x;
            out->y = in->y;
            out->z = in->z;
        }
        return 1.0f;
    }

    if (len2 == 0.0f)
    {
        if (out != in)
        {
            out->w = 1.0f;
            out->x = 0.0f;
            out->y = 0.0f;
            out->z = 0.0f;
        }
        return 0.0f;
    }

    float len = sqrtf(len2);
    float inv = 1.0f / len;
    out->w = in->w * inv;
    out->x = in->x * inv;
    out->y = in->y * inv;
    out->z = in->z * inv;
    return len;
}
