#include "types.h"

// sub_8277F310 -- a zeroing initialiser, no vtable. 56 B, 4 callers.
//
//      li      r11,0
//      stw     r11,8(r3)     <- emitted order is the source order
//      stw     r11,4(r3)        (MATCHED.md: store order is source order,
//      stw     r11,0(r3)         even when it is not address order)
//      stw     r11,24(r3)
//      stw     r11,20(r3)
//      stw     r11,16(r3)
//      stw     r11,12(r3)
//      stw     r11,40(r3)
//      stw     r11,44(r3)
//      stw     r11,48(r3)
//      stb     r11,52(r3)
//      stb     r11,53(r3)
//      blr
//
// Written in exactly the emitted order. Twelve independent stores is the
// shape that has resisted before (ctor_vt.cpp); attempted for the same
// reason -- it is cheap and the reading is unambiguous.

struct Zeroable
{
    /* 0x00 */ s32 f0;
    /* 0x04 */ s32 f4;
    /* 0x08 */ s32 f8;
    /* 0x0C */ s32 f12;
    /* 0x10 */ s32 f16;
    /* 0x14 */ s32 f20;
    /* 0x18 */ s32 f24;
    /* 0x1C */ char unk001C[12];
    /* 0x28 */ s32 f40;
    /* 0x2C */ s32 f44;
    /* 0x30 */ s32 f48;
    /* 0x34 */ u8  f52;
    /* 0x35 */ u8  f53;
};

void Clear(Zeroable* z)
{
    z->f8   = 0;
    z->f4   = 0;
    z->f0   = 0;
    z->f24  = 0;
    z->f20  = 0;
    z->f16  = 0;
    z->f12  = 0;
    z->f40  = 0;
    z->f44  = 0;
    z->f48  = 0;
    z->f52  = 0;
    z->f53  = 0;
}
