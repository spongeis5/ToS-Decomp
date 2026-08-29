// sub_82786628 -- push the movement since the last point, unless there has
// been none. 60 bytes, 3 callers. Same callee as
// src/m22_relative_point_call.cpp, 224 bytes earlier in the image.
//
//      lwz  r11,20(r3) ; lwz r10,12(r3)
//      cmpw cr6,r11,r10 ; bne- cr6,<push>
//      lwz  r9,24(r3) ; lwz r8,16(r3)
//      cmpw cr6,r9,r8 ; beqlr cr6
//  push:
//      lwz  r9,16(r3) ; li r6,0 ; lwz r8,24(r3)
//      subf r4,r11,r10
//      subf r5,r8,r9
//      b    0x82785F68
//      blr                          <- unreachable, and counted in the size
//
// Both compares are `cmpw` (signed) and the first branches to the push while
// the second returns, which is `if (last.x == x && last.y == y) return;` --
// the `&&` failing its first term jumps straight past the second.
//
// The +16 and +24 fields are RELOADED at the push even though the compare
// block had them: that block is skipped by the `bne-`, so the values do not
// dominate the join and MSVC has to re-read them. The +12 and +20 pair is
// loaded before the first branch and survives, which is why only two of the
// four reload.
//
// `subf rD,rA,rB` is rB - rA, so rA is the subtrahend: `x - last.x` and
// `y - last.y`, with the field subtracted from the current value.
//
// The tail branch is relocated, so 14 of 15 words are compared.

#include "types.h"

struct Tracker
{
    /* 0x00 */ u8  unk0000[0x0C];
    /* 0x0C */ s32 x;
    /* 0x10 */ s32 y;
    /* 0x14 */ s32 lastX;
    /* 0x18 */ s32 lastY;
};

ASSERT_OFFSET(Tracker, x, 0x0C);
ASSERT_OFFSET(Tracker, lastY, 0x18);

void PushPoint(Tracker* t, int dx, int dy, unsigned int flags);

void SyncPoint(Tracker* t)
{
    if (t->lastX == t->x && t->lastY == t->y)
        return;

    PushPoint(t, t->x - t->lastX, t->y - t->lastY, 0);
}
