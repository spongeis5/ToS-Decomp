#include "types.h"

// sub_82151C50 -- wrap an angle into [0, 2*pi). 48 B, 35 callers.
//   lis r11,-32256 ; lfs f0,11684(r11)      -> 82002DA4 = 0.0f
//   lis r11,-32256 ; fcmpu cr6,f1,f0
//   lfs f0,12156(r11)                       -> 82002F7C = 6.2831855f
//   bge- cr6,0x82151c70
//   fadds f1,f1,f0 ; blr                    fall-through: a < 0
// 0x82151c70:
//   fcmpu cr6,f1,f0 ; bltlr cr6             a < 2pi -> return a unchanged
//   fsubs f1,f1,f0 ; blr
//
// Both guards are written the polarity the branches give: the `bge-` skips
// the add, so `a < 0` is the fall-through and comes first; the `bltlr` is a
// conditional RETURN, so `return a` is the guard and the subtract is the
// body.
//
// The 2*pi constant is loaded into the SAME register the 0.0f compare just
// used, and both compares then read it, which is why only one load appears
// for the two `>= 2pi` / `< 2pi` uses.

static const float kTwoPi = 6.283185307179586f;   /* 0x40C90FDB */

float WrapAngleTwoPi(float a)
{
    if (a < 0.0f)
        return a + kTwoPi;
    if (a < kTwoPi)
        return a;
    return a - kTwoPi;
}
