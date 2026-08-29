// sub_827865F0 -- subtract an origin held in the object from an (x,y) pair
// and tail-call with a trailing zero. 24 bytes, 4 callers.
//
//      lwz     r11,24(r3)
//      li      r6,0
//      lwz     r10,20(r3)
//      subf    r5,r11,r5        r5 = y - o->originY
//      subf    r4,r10,r4        r4 = x - o->originX
//      b       0x82785F68
//
// `subf rD,rA,rB` computes rB - rA, so rA is the SUBTRAHEND: the field is
// subtracted from the parameter, not the other way round. That operand order
// is source-readable (unlike mullw or a commutative float), so it is worth
// reading rather than guessing.
//
// The callee sub_82785F68 takes four arguments -- it copies r4 and r5 into a
// 20-byte stack record twice over ({r4,r5,r4,r5,1}), compares r4 against zero
// with a SIGNED `cmpwi` (so it is an int) and stores r6 with `stb` after an
// unsigned `cmplw` against a byte (so that one is unsigned).
//
// The field at +24 is loaded FIRST and is the one used by the LATER argument,
// which is the right-to-left evaluation MSVC does for the register arguments;
// the source is written in argument order.
//
// The tail branch is relocated, so 5 of 6 words are compared.

#include "types.h"

struct Origin
{
    /* 0x00 */ char unk0000[0x14];
    /* 0x14 */ s32  originX;
    /* 0x18 */ s32  originY;
};

ASSERT_OFFSET(Origin, originX, 0x14);
ASSERT_OFFSET(Origin, originY, 0x18);

void PushPoint(Origin* o, int x, int y, unsigned int flags);

void PushPointRelative(Origin* o, int x, int y)
{
    PushPoint(o, x - o->originX, y - o->originY, 0);
}
