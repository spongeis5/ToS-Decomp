#include "types.h"

// sub_82691C50 -- 2x3 affine matrix, this = this * other, in place.
// 156 B, 18 callers.  r3 = this, r4 = other.  /O2 /Os.
//
// Layout a b tx / c d ty at 0 4 8 / 12 16 20; the mirror function
// sub_82691CF0 does other * this on the same layout.
//
//   out.a  = a*o.a  + b*o.c        stores go 0, 12, 4, 16, 8, 20 --
//   out.c  = c*o.a  + d*o.c        column by column, not row by row
//   out.b  = a*o.b  + b*o.d
//   out.d  = c*o.b  + d*o.d
//   out.tx = a*o.tx + b*o.ty + tx
//   out.ty = c*o.tx + d*o.ty + ty
//
// All six fields of `this` are loaded before the first store -- one pointer,
// distinct offsets, so MSVC knows they cannot alias each other -- and every
// field of `other` is reloaded after each store, which is the aliasing
// between two pointers of the same type that it could not remove. The
// product is computed in place, so the source has to snapshot first.
//
// THE SNAPSHOT IS AN ARRAY. With six scalar locals -- the obvious spelling,
// and the one that matched sub_82691CF0 as a free function -- the body is 31
// of 39 words: every load, every store and every opcode is right, and the
// eight `fmuls`/`fmadds` of the last four assignments come out with their two
// multiplicands in the opposite A/C slots. Nothing about the expressions
// reaches that. Fourteen shapes were tried at both levels -- member function,
// reference parameter, `Mtx23 s = *this`, const locals, locals declared late,
// locals in struct order, computing all six into locals before storing,
// reading the members directly wherever they still hold the old value,
// aliasing `o` through a second pointer, aliasing `this` through a second
// pointer -- and every one of them is 31/39 or worse. Writing the snapshot as
// `float s[6]` is 39 of 39.
//
// That also says the operand order cannot be read out of the source here.
// `a*o->b` versus `o->b*a`, and `a*o->b + b*o->d` versus `b*o->d + a*o->b`,
// compile to identical bytes: MSVC canonicalises both commutative operators
// under /fp:fast. What decides the A/C slot is how the value was PRODUCED --
// an array element and a scalar local are not the same thing to it.

struct Mtx23
{
    f32 a;  f32 b;  f32 tx;
    f32 c;  f32 d;  f32 ty;
};
ASSERT_OFFSET(Mtx23, tx, 0x08);
ASSERT_OFFSET(Mtx23, c, 0x0C);
ASSERT_OFFSET(Mtx23, d, 0x10);
ASSERT_OFFSET(Mtx23, ty, 0x14);

void Mtx23Mul(Mtx23* m, const Mtx23* o)
{
    float s[6];
    s[0] = m->a;  s[1] = m->b;  s[2] = m->tx;
    s[3] = m->c;  s[4] = m->d;  s[5] = m->ty;

    m->a  = s[0] * o->a  + s[1] * o->c;
    m->c  = s[3] * o->a  + s[4] * o->c;
    m->b  = s[0] * o->b  + s[1] * o->d;
    m->d  = s[3] * o->b  + s[4] * o->d;
    m->tx = s[0] * o->tx + s[1] * o->ty + s[2];
    m->ty = s[3] * o->tx + s[4] * o->ty + s[5];
}
