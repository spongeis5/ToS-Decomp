#include "types.h"

// sub_825FCCB8 -- zero an 8-word array, then three scattered fields.
// 44 B, 4 callers.
//
//      li      r10,8
//      li      r9,0
//      addi    r11,r3,-4        the biased pointer so stwu can store and
//      stw     r9,32(r3)        step in one instruction (idiom table)
//      mtctr   r10
// loop:
//      stwu    r9,4(r11)        w[0..7] = 0
//      bdnz+   loop
//      stw     r9,36(r3)
//      stw     r9,44(r3)
//      stw     r9,52(r3)
//      blr
//
// The loop survives (not unrolled) and stores 32..36 come out of source
// order: f36 first, then the loop, then 44 and 52.

struct Zed
{
    /* 0x00 */ u32  w[8];
    /* 0x20 */ s32  f32;
    /* 0x24 */ s32  f36;
    /* 0x28 */ char unk0028[4];
    /* 0x2C */ s32  f44;
    /* 0x30 */ char unk0030[4];
    /* 0x34 */ s32  f52;
};

ASSERT_OFFSET(Zed, f32, 32);
ASSERT_OFFSET(Zed, f36, 36);
ASSERT_OFFSET(Zed, f52, 52);

void ClearZed(Zed* z)
{
    z->f32 = 0;
    for (int i = 0; i < 8; ++i)
        z->w[i] = 0;
    z->f36 = 0;
    z->f44 = 0;
    z->f52 = 0;
}
