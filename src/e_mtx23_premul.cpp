#include "types.h"

// sub_82691CF0 -- 2x3 affine matrix, this = other * this, in place.
// 164 B, 14 callers.  r3 = this, r4 = other.  /O2 /Os.
//
// Mirror of sub_82691C50, 160 bytes further up the same translation unit:
// same layout, same store order (0, 12, 4, 16, 8, 20), but the operands of
// every product are swapped and the translation addend comes from `other`
// rather than from `this` -- which costs the two extra loads that make this
// 164 bytes against the other's 156.
//
//   out.a  = o.a*a  + o.b*c
//   out.c  = o.c*a  + o.d*c
//   out.b  = o.a*b  + o.b*d
//   out.d  = o.c*b  + o.d*d
//   out.tx = o.a*tx + o.b*ty + o.tx
//   out.ty = o.c*tx + o.d*ty + o.ty
//
// This one matches with the snapshot written EITHER as six scalar locals or
// as `float s[6]` -- both give 41 of 41. Its neighbour sub_82691C50 only
// matches with the array, so the array is what the shared source almost
// certainly is, and it is written that way here for that reason rather than
// because this function can tell the difference.

struct Mtx23
{
    f32 a;  f32 b;  f32 tx;
    f32 c;  f32 d;  f32 ty;
};
ASSERT_OFFSET(Mtx23, tx, 0x08);
ASSERT_OFFSET(Mtx23, c, 0x0C);
ASSERT_OFFSET(Mtx23, d, 0x10);
ASSERT_OFFSET(Mtx23, ty, 0x14);

void Mtx23PreMul(Mtx23* m, const Mtx23* o)
{
    float s[6];
    s[0] = m->a;  s[1] = m->b;  s[2] = m->tx;
    s[3] = m->c;  s[4] = m->d;  s[5] = m->ty;

    m->a  = o->a * s[0] + o->b * s[3];
    m->c  = o->c * s[0] + o->d * s[3];
    m->b  = o->a * s[1] + o->b * s[4];
    m->d  = o->c * s[1] + o->d * s[4];
    m->tx = o->a * s[2] + o->b * s[5] + o->tx;
    m->ty = o->c * s[2] + o->d * s[5] + o->ty;
}
