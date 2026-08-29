// sub_825BE3D8 -- zero five consecutive words and return 0. 36 B, 4 callers.
//
//      mr      r10,r3           the object moves OUT of r3
//      li      r11,0
//      li      r3,0             the return value, materialised early
//      stw     r11,8(r10)
//      stw     r11,12(r10)
//      stw     r11,16(r10)
//      stw     r11,20(r10)
//      stw     r11,24(r10)
//      blr
//
// The `mr r10,r3` exists only because r3 is the RETURN REGISTER: the constant
// 0 is materialised into it and the object pointer has to move out of the
// way. A void function would keep the base in r3 and be one instruction
// shorter. Same reading as src/u3_init_zero.cpp (sub_825BE440), which is
// 104 bytes further on in the same neighbourhood and has the identical
// mr/li/li opening.
//
// Five separate `stw`s rather than a loop, so this is written out field by
// field; the fields are 4-byte and the base is 8-aligned, but nothing here
// says whether they are ints, floats or pointers -- they are zeroed with a
// GPR, which every 4-byte type does.
//
// Nothing is relocated; all 9 words are compared.

#include "types.h"

struct ZeroFive
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ s32  f08;
    /* 0x0C */ s32  f0C;
    /* 0x10 */ s32  f10;
    /* 0x14 */ s32  f14;
    /* 0x18 */ s32  f18;
};
ASSERT_OFFSET(ZeroFive, f08, 0x08);
ASSERT_OFFSET(ZeroFive, f18, 0x18);

int ZeroFiveReset(ZeroFive* o)
{
    o->f08 = 0;
    o->f0C = 0;
    o->f10 = 0;
    o->f14 = 0;
    o->f18 = 0;
    return 0;
}
