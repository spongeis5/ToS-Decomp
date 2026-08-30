#include "types.h"

// sub_82608010 -- push a 64-bit value onto a global array, post-increment.
// 52 B, 3 callers.
//
//      stw     r3,-16(r1)       the __int64 parameter arrives as the
//      stw     r4,-12(r1)       r3:r4 pair; MSVC rejoins it through the
//      ld      r6,-16(r1)       red zone -- a spill, not a round-trip idiom
//      lwz     r11,0(g)         count
//      addi    r11,r11,1
//      lwz     r10,4(g)         items
//      rlwinm  r9,r11,3,0,28    newCount * 8
//      stw     r11,0(g)         count = newCount
//      add     r5,r9,r10
//      std     r6,-8(r5)        items[newCount-1] = v
//      blr

struct PushCtx
{
    /* 0x00 */ s32   count;
    /* 0x04 */ s64*  items;
};

ASSERT_OFFSET(PushCtx, count, 0);
ASSERT_OFFSET(PushCtx, items, 4);

extern PushCtx g_push_82A352E0;

void Push64(s64 v)
{
    g_push_82A352E0.items[g_push_82A352E0.count++] = v;
}

// NEAR-MISS. parameter pair rejoined by ld from red zone; source must spill the s64 arg the same way.
