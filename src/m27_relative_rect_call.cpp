// sub_82786608 -- the sibling of sub_827865F0, 24 bytes later in the image.
// Same origin subtraction, plus a second point turned into a DELTA.
// 32 bytes, 4 callers.
//
//      lwz     r11,24(r3)
//      subf    r7,r5,r7           r7 = y1 - y0     (uses the ORIGINAL r5)
//      lwz     r10,20(r3)
//      subf    r6,r4,r6           r6 = x1 - x0     (uses the ORIGINAL r4)
//      li      r8,0
//      subf    r5,r11,r5          r5 = y0 - o->originY
//      subf    r4,r10,r4          r4 = x0 - o->originX
//      b       0x82786280
//
// The ordering is forced, not chosen: both deltas are computed BEFORE r4 and
// r5 are overwritten with the origin-relative values, so the two second-point
// arguments really are differences of the two parameter pairs and not of the
// already-adjusted ones. Any other reading would need a copy that is not
// there.
//
// `subf rD,rA,rB` is rB - rA, so rA is the subtrahend in all four: the field
// is subtracted from the parameter, and the first point from the second.
//
// The callee sub_82786280 takes six arguments and is the four-argument
// sub_827865F0 callee's twin -- it builds the same 20-byte stack record from
// r4..r7 and stores a 0 byte where the other stores a 1.
//
// The tail branch is relocated, so 7 of 8 words are compared.

#include "types.h"

struct Origin2
{
    /* 0x00 */ char unk0000[0x14];
    /* 0x14 */ s32  originX;
    /* 0x18 */ s32  originY;
};

ASSERT_OFFSET(Origin2, originX, 0x14);
ASSERT_OFFSET(Origin2, originY, 0x18);

void PushSpan(Origin2* o, int x, int y, int dx, int dy, unsigned int flags);

void PushSpanRelative(Origin2* o, int x0, int y0, int x1, int y1)
{
    PushSpan(o, x0 - o->originX, y0 - o->originY, x1 - x0, y1 - y0, 0);
}
