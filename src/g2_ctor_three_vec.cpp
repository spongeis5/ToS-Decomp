// sub_8219E748 -- constructor: five scalar fields and three copies of the
// same 12-byte constant vector, then -1. 144 B, 5 callers.  r4 is unused.
//
//      lis/lis ; addi r7,r9,11376 = 82002C70  ({0,0,0} in .rdata)
//      li r11,0 ; li r5,1 ; mr r10,r3            r3 saved: it is LIVE OUT
//      stw r11,0(r3) ; lfs f0,11684(r8) = 82002DA4 (0.0f)
//      stw r11,4(r3) ; stw r11,16(r3) ; stfs f0,8(r3) ; stw r5,12(r3)
//      then three lwz/stw triples from r7 into +20, +32, +44
//      li r11,-1 ; mr r3,r10 ; ... ; stw r11,56(r10)
//
// `mr r3,r10` restores the object pointer into r3 halfway through and nothing
// overwrites it, so r3 is the return value: this is a CONSTRUCTOR, which is
// the same lever as src/c_share_static.cpp -- r3 live out.
//
// The integer stores go 0, 4, 16, 12 and the single float store lands
// between 16 and 12, which is the two-stream interleave from
// src/j_reset_state.cpp: each stream's internal order is source order.
//
// The three copies each RE-LOAD all three words from the global rather than
// reusing the first copy's registers, and two of them get their own `mr` of
// the source pointer -- the pointless-move fingerprint of a repeated
// subexpression, here one struct assignment written three times.

#include "types.h"

struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};
ASSERT_SIZE(Vec3, 12);

extern const Vec3 kZero3;   /* 82002C70 */

struct Node3
{
    /* 0x00 */ s32  f00;
    /* 0x04 */ s32  f04;
    /* 0x08 */ f32  f08;
    /* 0x0C */ s32  f12;
    /* 0x10 */ s32  f16;
    /* 0x14 */ Vec3 a;
    /* 0x20 */ Vec3 b;
    /* 0x2C */ Vec3 c;
    /* 0x38 */ s32  f56;

    Node3();
};
ASSERT_OFFSET(Node3, a, 20);
ASSERT_OFFSET(Node3, b, 32);
ASSERT_OFFSET(Node3, c, 44);
ASSERT_OFFSET(Node3, f56, 56);

Node3::Node3()
{
    f00 = 0;
    f04 = 0;
    f16 = 0;
    f08 = 0.0f;
    f12 = 1;
    a = kZero3;
    b = kZero3;
    c = kZero3;
    f56 = -1;
}
