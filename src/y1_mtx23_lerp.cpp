#include "types.h"

// sub_82691928 -- interpolate two 2x3 affine matrices, COLUMN BY COLUMN.
// 124 B.  Bridge between 826918F8 (ResetBasis) and 826919A8
// (Mtx23::TransformPoint), so the layout is the one d_basis_identity.cpp
// and e_mtx23_xform.cpp already fixed: a,b,tx at 0,4,8 and c,d,ty at
// 12,16,20.
//
//   lfs f0,0(r4) ; lfs f13,0(r5) ; fsubs f13,f13,f0
//   fmadds f0,f13,f1,f0 ; stfs f0,0(r3)
//
// fmadds is (b - a) * t + a, so the expression is a + (b - a) * t.
//
// Six identical five-instruction blocks with NO scheduling overlap at all,
// so the emitted store order 0, 12, 4, 16, 8, 20 is source order: the two
// rows are written together one COLUMN at a time.

struct Mtx23
{
    f32 a;  f32 b;  f32 tx;
    f32 c;  f32 d;  f32 ty;
};
ASSERT_OFFSET(Mtx23, b,  0x04);
ASSERT_OFFSET(Mtx23, tx, 0x08);
ASSERT_OFFSET(Mtx23, c,  0x0C);
ASSERT_OFFSET(Mtx23, d,  0x10);
ASSERT_OFFSET(Mtx23, ty, 0x14);

void Mtx23Lerp(Mtx23* o, const Mtx23* p, const Mtx23* q, f32 t)
{
    o->a  = p->a  + (q->a  - p->a)  * t;
    o->c  = p->c  + (q->c  - p->c)  * t;
    o->b  = p->b  + (q->b  - p->b)  * t;
    o->d  = p->d  + (q->d  - p->d)  * t;
    o->tx = p->tx + (q->tx - p->tx) * t;
    o->ty = p->ty + (q->ty - p->ty) * t;
}
