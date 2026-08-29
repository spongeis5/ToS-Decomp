#include "types.h"

// sub_82858720 -- push a 2-D point through three 2x3 affine transforms: a
// fixed one, one SELECTED by which side of a rectangle the point landed on,
// and a fixed one after. 316 B, 6 callers, 54 float ops.
//
// The transform is the same six instructions three times:
//
//      lfs f13,56(r3) ; fmuls f13,f13,f0          a * x
//      lfs f11,60(r3) ; fmadds f13,f11,f12,f13    + b * y
//      lfs f10,64(r3) ; fadds f13,f13,f10         + c
//      stfs f13,0(r4)
//
// so the element layout is { a b c | d e f } at 0/4/8 | 12/16/20, 24 bytes.
// *px is RELOADED before each later use while *py is kept in f0 -- the store
// through py may alias px, and the value just written through py is known.
// That reload is aliasing, not a spelling choice, and it is what makes one
// inlined helper reproduce all three copies.
//
// The selector is a four-bit Cohen-Sutherland outcode built by a chain of
// `rlwinm rX,rX,1,0,30 ; or`, which is the accumulate spelling
// `code = (code << 1) | term` and not `b0 | b1<<1 | b2<<2 | b3<<3` -- the
// latter would emit three shifts of three different widths. The leading
// `0 << 1 | b3` folds away, leaving exactly three shifts and three ors.
//
// Each term is materialised, not branched: `li 1 ; fcmpu ; bgt- ; li 0` is
// MSVC turning a comparison into an int. The `li r8,1` for the FIRST term is
// hoisted to the second instruction of the function, ahead of all the float
// work.
//
//      lbzx  r11,r11,r10        the 16-entry byte table at 8208D280
//      addi  r11,r11,6
//      mulli r11,r11,24
//      add   r11,r11,r3
//
// `(t + 6) * 24` is the array at byte offset 144 with the offset folded into
// the INDEX -- 6 * 24 == 144. The same fold appears bare in sub_821A6B38.

struct Mtx23
{
    /* 0x00 */ f32 a;
    /* 0x04 */ f32 b;
    /* 0x08 */ f32 c;
    /* 0x0C */ f32 d;
    /* 0x10 */ f32 e;
    /* 0x14 */ f32 f;
};
ASSERT_SIZE(Mtx23, 24);

struct Warp
{
    /* 0x00 */ char  unk0000[0x38];
    /* 0x38 */ Mtx23 pre;
    /* 0x50 */ char  unk0050[0x18];
    /* 0x68 */ f32   loX;
    /* 0x6C */ f32   loY;
    /* 0x70 */ f32   hiX;
    /* 0x74 */ f32   hiY;
    /* 0x78 */ Mtx23 post;
    /* 0x90 */ Mtx23 mats[9];
};
ASSERT_OFFSET(Warp, pre,  0x38);
ASSERT_OFFSET(Warp, loX,  0x68);
ASSERT_OFFSET(Warp, hiY,  0x74);
ASSERT_OFFSET(Warp, post, 0x78);
ASSERT_OFFSET(Warp, mats, 0x90);

extern const u8 g_warpRegion[];

static void Xform(const Mtx23* m, f32* px, f32* py)
{
    f32 x = *px;
    f32 y = *py;

    *px = m->a * x + m->b * y + m->c;
    *py = m->d * x + m->e * y + m->f;
}

void WarpPoint(Warp* w, f32* px, f32* py)
{
    Xform(&w->pre, px, py);

    int c0 = (*px > w->hiX);
    int c1 = (*py > w->hiY);
    int c2 = (*px < w->loX);
    int c3 = (*py < w->loY);
    int code = (((c3 << 1 | c2) << 1 | c1) << 1) | c0;

    Xform(&w->mats[g_warpRegion[code]], px, py);
    Xform(&w->post, px, py);
}
