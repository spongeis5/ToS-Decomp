#include "types.h"

// sub_82202BC8 -- store one float into four fields and zero a fifth.
// 28 B, 5 callers.
//   li r11,0 ; stfs f1,60(r3) ; stfs f1,64(r3) ; stfs f1,68(r3)
//   stw r11,76(r3) ; stfs f1,72(r3) ; blr
// Store order 60, 64, 68, 76, 72 -- the integer store is scheduled between
// the fourth and fifth float stores, and writing it in that order is what
// reproduces it.
struct Quad
{
    char unk0000[0x3C];
    f32 a; f32 b; f32 c; f32 d;
    s32 n;
};
ASSERT_OFFSET(Quad, a, 0x3C);
ASSERT_OFFSET(Quad, d, 0x48);
ASSERT_OFFSET(Quad, n, 0x4C);
void FillQuad(Quad* q, float v)
{
    q->a = v; q->b = v; q->c = v;
    q->n = 0;
    q->d = v;
}
