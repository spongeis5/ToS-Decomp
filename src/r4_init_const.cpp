#include "types.h"

// sub_821FAE48 -- constructor: a vtable, one large integer constant and two
// ones. 48 B, 7 callers.
//
//      lis     r9,-32256
//      li      r10,0
//      li      r11,1
//      lis     r8,15360        0x3C000000, ONE lis -- no ori, no float pool
//      stw     r10,8(r3)       +8  = 0
//      addi    r7,r9,17184     = 82004320
//      stw     r10,12(r3)      +12 = 0
//      stw     r8,4(r3)        +4  = 0x3C000000
//      stw     r7,0(r3)        +0  = &kVTable_82004320
//      stw     r11,16(r3)      +16 = 1
//      stw     r11,20(r3)      +20 = 1
//      blr
//
// The +4 word is built with a bare `lis`, so it is an INTEGER constant: every
// float constant in this image comes out of the literal pool with `lfs`
// (compare src/j_reset_range.cpp, where 1.0f and 0.0f both do). Measured, not
// assumed -- all 24 store orders with `f32 f04 = 0.0078125f` score 0 of 12.
//
// STORE ORDER IS *NOT* SOURCE ORDER HERE, and this is a third exception to
// that rule. The emitted order is 8, 12, 4, 0, 16, 20; the source is plain
// ADDRESS order. Writing the source in the emitted order gives 7 of 12 with
// the two materialised values swapped between r7 and r8 -- the vtable's
// lis/addi pair completes first and its store is dragged ahead of the
// constant's. All 24 permutations of the first four assignments were
// compiled: 0-4-8-12, 4-0-8-12, 4-8-0-12 and 4-8-12-0 all match, and every
// order that puts +8 or +12 first does not. So what the compiler fixes is
// that the two ZERO stores go last of the four; among the rest it is free.

struct VT821FAE48;
extern const VT821FAE48 kVTable_82004320;

struct Init48
{
    /* 0x00 */ const VT821FAE48* vt;
    /* 0x04 */ s32 f04;
    /* 0x08 */ s32 f08;
    /* 0x0C */ s32 f0C;
    /* 0x10 */ s32 f10;
    /* 0x14 */ s32 f14;
};
ASSERT_OFFSET(Init48, f14, 0x14);

void Construct(Init48* o)
{
    o->vt  = &kVTable_82004320;
    o->f04 = 0x3C000000;
    o->f08 = 0;
    o->f0C = 0;
    o->f10 = 1;
    o->f14 = 1;
}
