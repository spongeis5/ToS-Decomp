#include "types.h"

// sub_8226C2D8 -- two rounds of zero-plus-constant writes over interleaved
// fields. 60 B, 3 callers.
//
//   li 2 ; mtctr ; li 0 ; f0 = 0.0f (82002DA4)
//   loop (2x):
//     stw 0 -> r11-4, r11-12   (two u32s behind the cursor)
//     stfsu f0,4(r11)          (cursor steps +4)
//     stfs f0 -> r10+0,+4,+8   (fixed: 352/356/360, written twice)
//   r11 starts at r3+340.
//
// The two stw targets walk 336/328 then 340/332 -- descending relative to
// an ascending float cursor -- so the source writes p[-2], p[-3], then the
// float at p[0] with p stepping forward; the fixed triple is loop-invariant
// but emitted inside the loop.

extern const float kZero_82002DA4;

struct Loop60
{
    /* 0x14C */ char  unk0000[328];
    /* 0x148 */ s32   a[4];
    /* 0x158 */ float f[2];
    /* 0x160 */ float fixed[3];
};

ASSERT_OFFSET(Loop60, f, 344);
ASSERT_OFFSET(Loop60, fixed, 352);

void FillLoop60(Loop60* t)
{
    for (int i = 0; i < 2; ++i)
    {
        t->a[2 + i] = 0;
        t->a[i] = 0;
        t->f[i] = 0.0f;
        t->fixed[0] = 0.0f;
        t->fixed[1] = 0.0f;
        t->fixed[2] = 0.0f;
    }
}

// NEAR-MISS. descending u32 pair vs ascending float cursor; stfsu shape not reproduced.
