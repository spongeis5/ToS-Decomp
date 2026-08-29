#include "types.h"

// sub_821FC4D8 -- advance a scalar toward a ceiling while the reference point
// is inside a radius, and snap it to 1.0f when it is not. 88 B, 6 callers.
// r3 = state, r4 = point, f1 = the step scale.
//
//      lfs     f0,4(r4)          v->y
//      fmuls   f13,f0,f0
//      lfs     f12,0(r4)         v->x
//      lfs     f11,8(r4)         v->z
//      lfs     f10,0(r3)         s->limit
//      fmadds  f9,f12,f12,f13    x*x + y*y
//      fmadds  f8,f11,f11,f9     + z*z
//      fcmpu   cr6,f8,f10
//      bge-    cr6,outside
//      lfs     f0,4(r3)          s->rate
//      lfs     f13,12(r3)        s->value
//      fmadds  f13,f0,f1,f13     t = rate * step + value
//      lfs     f0,8(r3)          s->ceiling
//      fcmpu   cr6,f13,f0
//      bge-    cr6,store
//      fmr     f0,f13            <- DEAD
//      stfs    f13,12(r3)
//      blr
// outside:
//      lis     r11,-32256
//      lfs     f0,11584(r11)     = 0x82002D40 = 1.0f
// store:
//      stfs    f0,12(r3)
//      blr
//
// THE DEAD `fmr f0,f13` IS THE WHOLE SPECIFICATION. It is the phi copy for a
// SINGLE local that all three paths assign: the clamped path and the outside
// path both leave the value in f0, so the fall-through has to move f13 there
// before joining. MSVC then tail-duplicated the store into that block using
// the original register and never removed the copy it had just made.
//
// So the source has ONE store at the end and one variable, not three
// assignments to s->value -- three assignments give three independent stores
// and no `fmr` anywhere.
//
// The accumulation order is readable even though the multiply operand order is
// not: `(x*x + y*y) + z*z` seeds the chain with the SECOND term (`fmuls` on y)
// and folds the first into the `fmadds`. Under /fp:fast the A/C slots of a
// commutative float multiply carry no information, so nothing is spent there.
//
// 0x82002D40 is the same 1.0f src/j_inv_or_clamp.cpp loads.
//
// ---------------------------------------------------------------------
// 13 of 19 words, up from 8 of 19, and the six that differ are ONE MISSING
// INSTRUCTION -- the dead `fmr f0,f13` -- with everything after it shifted
// by a word. Every instruction before 821FC514 is now identical.
//
// WHAT MOVED IT: THE INNER CLAMP'S FAST PATH IS AN EARLY STORE AND RETURN,
// NOT AN ASSIGNMENT TO THE SHARED LOCAL. That decides which of `n` and
// `ceiling` gets the phi register, and everything else follows:
//
//   t = (n < ceiling) ? n : ceiling;      lfs f13,12(r3) then lfs f0,4(r3);
//   (and every other single-store form)   fmadds f0,... ; fcmpu cr6,f0,f13 ;
//                                         blt- -- n is in the phi register
//                                         f0, ceiling is spilled to f13,
//                                         and the CEILING store is the one
//                                         MSVC tail-duplicates.  8 of 19.
//
//   if (n < ceiling) { s->value = n;      lfs f0,4(r3) then lfs f13,12(r3);
//                      return; }          fmadds f13,... ; fcmpu cr6,f13,f0 ;
//   t = ceiling;                          bge- -- the target's registers,
//                                         its compare operand order and its
//                                         branch polarity exactly.  13 of 19.
//
// So the register assignment inside the clamp is decided by whether the
// fast path RETURNS or falls into the shared tail, and not by the operand
// order of the comparison, the polarity of the `if`, or which value is
// named in a local. Ten single-store spellings were measured -- ternary
// both ways, if/else both ways, assign-then-clamp, `!(n < c)`, a named
// `c`, a min() helper, and the outer test inverted -- and all ten give
// `fmadds f0` and 84 bytes.
//
// WHAT IS LEFT is the dead `fmr f0,f13` alone. It is the phi copy for the
// fast path, which this shape no longer has -- the early return stores f13
// and leaves. So the target wants BOTH: the phi copy of a single-store
// source AND the register assignment of an early-return one. /O2 /Os
// produces the copy (`fmr f0,f13` then `b`) but coalesces the whole
// length-squared chain into f0, which costs the first eight words; no
// spelling of the dot product recovers the fresh-register form at that
// level. So the two halves are available at different optimisation levels
// and not together, which is the same shape as sub_826973C8's cr6/GPR
// split.

struct ClampState
{
    /* 0x00 */ f32 limit;
    /* 0x04 */ f32 rate;
    /* 0x08 */ f32 ceiling;
    /* 0x0C */ f32 value;
};
ASSERT_OFFSET(ClampState, rate,    0x04);
ASSERT_OFFSET(ClampState, ceiling, 0x08);
ASSERT_OFFSET(ClampState, value,   0x0C);

struct Point3
{
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
};
ASSERT_OFFSET(Point3, z, 0x08);

void ClampAdvance(ClampState* s, const Point3* v, float step)
{
    float t;

    if (v->x * v->x + v->y * v->y + v->z * v->z < s->limit)
    {
        float n = s->rate * step + s->value;
        if (n < s->ceiling)
        {
            s->value = n;
            return;
        }
        t = s->ceiling;
    }
    else
    {
        t = 1.0f;
    }

    s->value = t;
}
