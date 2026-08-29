// sub_82784DE0 -- zero thirteen words and two bytes of a record.
// 68 B, 5 callers.
//
//      li      r11,0
//      stw     r11,28(r3)   stw r11,32(r3)   stw r11,36(r3)
//      stw     r11,8(r3)    stw r11,4(r3)    stw r11,0(r3)
//      stw     r11,24(r3)   stw r11,20(r3)   stw r11,16(r3)
//      stw     r11,12(r3)
//      stw     r11,40(r3)   stw r11,44(r3)   stw r11,48(r3)
//      stb     r11,52(r3)
//      stb     r11,53(r3)
//      blr
//
// Store order is source order, so the odd sequence -- 0x1C,0x20,0x24 then
// 0x08,0x04,0x00 then 0x18,0x14,0x10,0x0C then 0x28,0x2C,0x30 -- is written
// out exactly as emitted rather than tidied into address order. Two of the
// four runs descend, which no scheduler would produce from ascending source.
// src/zero5_20first.cpp and src/u3_init_zero.cpp are the same lever.
//
// The last two stores are `stb`, so those two fields are byte-wide; the
// thirteen before them are `stw` and none is merged into a `std`, so they are
// separate 32-bit members rather than 64-bit ones.
//
// r3 is never written, so the object pointer is also whatever this returns --
// a `void` free function and a constructor are indistinguishable here, and
// the simpler one is written.
//
// Nothing is relocated; all 17 words are compared.

#include "types.h"

struct Record
{
    /* 0x00 */ s32 f00;
    /* 0x04 */ s32 f04;
    /* 0x08 */ s32 f08;
    /* 0x0C */ s32 f0C;
    /* 0x10 */ s32 f10;
    /* 0x14 */ s32 f14;
    /* 0x18 */ s32 f18;
    /* 0x1C */ s32 f1C;
    /* 0x20 */ s32 f20;
    /* 0x24 */ s32 f24;
    /* 0x28 */ s32 f28;
    /* 0x2C */ s32 f2C;
    /* 0x30 */ s32 f30;
    /* 0x34 */ u8  f34;
    /* 0x35 */ u8  f35;
};

ASSERT_OFFSET(Record, f1C, 0x1C);
ASSERT_OFFSET(Record, f28, 0x28);
ASSERT_OFFSET(Record, f34, 0x34);
ASSERT_OFFSET(Record, f35, 0x35);

void RecordClear(Record* r)
{
    r->f1C = 0;
    r->f20 = 0;
    r->f24 = 0;

    r->f08 = 0;
    r->f04 = 0;
    r->f00 = 0;

    r->f18 = 0;
    r->f14 = 0;
    r->f10 = 0;
    r->f0C = 0;

    r->f28 = 0;
    r->f2C = 0;
    r->f30 = 0;

    r->f34 = 0;
    r->f35 = 0;
}
