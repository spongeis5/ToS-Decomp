// sub_821F7B18 -- zero two floats and two words, and raise a byte flag.
// 40 B, 4 callers.
//
//      lis     r10,-32256
//      li      r11,0
//      li      r9,1
//      stw     r11,128(r3)      +0x80 = 0
//      stw     r11,132(r3)      +0x84 = 0
//      lfs     f0,11684(r10)    = 0x82002DA4
//      stb     r9,137(r3)       +0x89 = 1
//      stfs    f0,120(r3)       +0x78
//      stfs    f0,124(r3)       +0x7C
//      blr
//
// THE CONSTANT AT 0x82002DA4 IS 0.0f -- read out of the image, word
// 0x00000000, not guessed from the instruction. A float zero has to come out
// of the literal pool on this target because there is no GPR-to-FPR move, so
// `lfs` here is not evidence of an interesting value; src/j_reset_range.cpp
// loads 0.0f the same way.
//
// Integer and float stores are TWO STREAMS interleaved by dual-issue
// scheduling (MATCHED.md), and each stream's internal order is source order:
// the integers run 0x80, 0x84, 0x89 and the floats run 0x78, 0x7C. Plain
// ascending address order satisfies both, and the interleaving between them
// is the compiler filling the gap while the `lis`/`lfs` pair is in flight --
// the same exception recorded for sub_826731B0, where all five source
// orderings gave identical bytes.
//
// +0x89 is written with `stb`, so it is a byte; +0x80 and +0x84 with `stw`
// from a zeroed GPR, which says 4 bytes and nothing about the type.
//
// The lis/lfs pair is relocated; the other 8 words are compared.

#include "types.h"

struct ResetTarget
{
    /* 0x00 */ char unk0000[0x78];
    /* 0x78 */ f32  x;
    /* 0x7C */ f32  y;
    /* 0x80 */ s32  f80;
    /* 0x84 */ s32  f84;
    /* 0x88 */ char unk0088[0x01];
    /* 0x89 */ u8   ready;
};
ASSERT_OFFSET(ResetTarget, x,     0x78);
ASSERT_OFFSET(ResetTarget, f80,   0x80);
ASSERT_OFFSET(ResetTarget, ready, 0x89);

void ResetTargetInit(ResetTarget* t)
{
    t->x     = 0.0f;
    t->y     = 0.0f;
    t->f80   = 0;
    t->f84   = 0;
    t->ready = 1;
}
