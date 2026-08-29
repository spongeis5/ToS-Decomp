#include "types.h"

// sub_825BE440 -- record an owner pointer, zero eleven fields, set one to -1,
// and return 0. 68 B, 6 callers.
//
//      mr      r10,r3
//      stw     r4,336(r3)     +0x150 = owner
//      li      r11,0
//      li      r9,-1
//      stw     r11,8(r3)      +0x08
//      li      r3,0           the return value, materialised early
//      stw     r11,12(r10)    +0x0C
//      stw     r11,28(r10)    +0x1C
//      stw     r11,32(r10)    +0x20
//      stw     r11,36(r10)    +0x24
//      stw     r11,324(r10)   +0x144
//      stw     r11,328(r10)   +0x148
//      stw     r11,332(r10)   +0x14C
//      stw     r9,340(r10)    +0x154 = -1
//      std     r11,344(r10)   +0x158, 64-bit
//      std     r11,352(r10)   +0x160, 64-bit
//      blr
//
// The `mr r10,r3` exists only because r3 is the RETURN REGISTER: the constant
// 0 is materialised into it early and the object pointer has to move out of
// the way. A void function would keep everything in r3.
//
// The two `std`s are 8-byte stores of a zeroed GPR, so those are 64-bit
// fields and not pairs of words -- both are 8-aligned, which two independent
// 32-bit stores would not have to be.
//
// Store order is source order, which is what fixes the odd 0x150-first
// sequence.

struct InitTarget
{
    /* 0x000 */ char  unk0000[0x08];
    /* 0x008 */ s32   f008;
    /* 0x00C */ s32   f00C;
    /* 0x010 */ char  unk0010[0x0C];
    /* 0x01C */ s32   f01C;
    /* 0x020 */ s32   f020;
    /* 0x024 */ s32   f024;
    /* 0x028 */ char  unk0028[0x11C];
    /* 0x144 */ s32   f144;
    /* 0x148 */ s32   f148;
    /* 0x14C */ s32   f14C;
    /* 0x150 */ void* owner;
    /* 0x154 */ s32   f154;
    /* 0x158 */ s64   f158;
    /* 0x160 */ s64   f160;
};
ASSERT_OFFSET(InitTarget, f008,  0x008);
ASSERT_OFFSET(InitTarget, f01C,  0x01C);
ASSERT_OFFSET(InitTarget, f144,  0x144);
ASSERT_OFFSET(InitTarget, owner, 0x150);
ASSERT_OFFSET(InitTarget, f158,  0x158);
ASSERT_OFFSET(InitTarget, f160,  0x160);

int InitTargetReset(InitTarget* o, void* owner)
{
    o->owner = owner;
    o->f008  = 0;
    o->f00C  = 0;
    o->f01C  = 0;
    o->f020  = 0;
    o->f024  = 0;
    o->f144  = 0;
    o->f148  = 0;
    o->f14C  = 0;
    o->f154  = -1;
    o->f158  = 0;
    o->f160  = 0;
    return 0;
}
