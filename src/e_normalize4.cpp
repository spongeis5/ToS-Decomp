#include "types.h"
#include <math.h>

// sub_82155080 -- normalise a quaternion from `in` into `out` and return the
// length it had. 204 B, 16 callers.  r3 = out, r4 = in.  /O2.
//
// MATCHED: 47 of 47 non-relocated words (4 more are relocated pool loads).
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
//     only one that puts the stores where the target has them.
//
//   * The equality tests are `==`, not an epsilon: `fcmpu` + `bne-` with no
//     fsubs/fabs in front. Compare sub_8214D640, which does use an epsilon
//     and spends four extra instructions on it.
//
// ---- what the last two words were, and the lever that reached them ----
//
// The residue was two `fmuls` whose multiplicands sat in the opposite A/C
// slots:
//
//     82155118  want fmuls f11,f13,f12    (in->w, inv)
//     82155130  want fmuls f7,f12,f8      (inv, in->y)
//
// so the target's four scaling multiplies are, in slot order,
//
//     w (comp, inv)   x (comp, inv)   y (inv, comp)   z (inv, comp)
//
// -- a clean split after the second, whereas every ordinary spelling gives
// the palindrome (inv, comp) (comp, inv) (comp, inv) (inv, comp).
//
// **THE SLOT FOLLOWS HOW THE SCALE FACTOR WAS PRODUCED, AND `x / len` IS A
// DIFFERENT PRODUCTION FROM `x * inv` EVEN THOUGH /fp:fast COMPILES THEM TO
// THE SAME INSTRUCTION.** MSVC really does fold four divisions by one value
// into a single `fdivs` plus four `fmuls` -- the nineteen instructions are
// byte-identical either way -- but the reciprocal reaches the multiply as a
// value the source named, and the division reaches it as a value the
// compiler invented, and the two land in opposite slots. So the split in the
// image is a split in the SOURCE: the first two components are scaled by a
// named reciprocal and the last two are divided.
//
// That alone is not enough. It also needs the four components staged through
// ONE AGGREGATE LOCAL, which is the lever this file already carried for the
// `w` multiply. Measured, and the two ingredients are independent:
//
//     staged, *inv w,x, /len y,z          47 of 47   <- this file
//     staged, *inv all four               46 of 47   (y wrong)
//     staged, /len all four               45 of 47   (w and x wrong)
//     staged, /len w,x, *inv y,z          44 of 47   (the split reversed)
//     NOT staged, *inv w,x, /len y,z      45 of 47   (w and y wrong)
//     four scalar temps, *inv w,x, /len y,z  45 of 47
//     one array SLOT reused for all four  45 of 47
//     staged w,x,y but not z              46 of 47
//
// so the aggregate must hold more than one live element, and the split must
// be that way round. Neither ingredient alone gets past 46.
//
// WHAT THE BYTES DO NOT SAY is which aggregate. `float s[4]`, a
// `union { float f[4]; }` view and a `Quat` local are all 47 of 47 and
// byte-identical; four separate scalars are 45. And `s[k] * (1.0f / len)`
// written out at the last two sites is byte-identical to `s[k] / len`, so
// the source may have spelled the division either way. The claim the file
// makes is therefore "one aggregate, and the last two scaled by a value the
// source did not name", and no more.
//
// Ruled out and not worth re-trying: source ORDER of any multiply (all 16
// combinations are byte-identical -- /fp:fast canonicalises commutative
// float operands), the len2 sum's order, 2304 flag combinations
// (`flagsweep.py --full`: 516 give 46 of 51, none better; every /fp:precise
// variant is 0 of 51 and every /Os variant is 4 of 51 at 200 bytes), and the
// twelve staging structures and nineteen y-site spellings recorded in the
// previous revision of this comment -- all of which held `inv` fixed as the
// scale for every component, which is why none of them could reach y.

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
    float s[4];

    s[3] = in->w;  out->w = s[3] * inv;
    s[0] = in->x;  out->x = s[0] * inv;
    s[1] = in->y;  out->y = s[1] / len;
    s[2] = in->z;  out->z = s[2] / len;
    return len;
}
