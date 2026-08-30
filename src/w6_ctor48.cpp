#include "types.h"

// sub_82155938 -- build a 26-byte record, then tail-call; the only argument
// move is r5 <- r9 (the seventh argument takes the third slot). 48 B, 4
// callers.
//
//      li      r11,0
//      sth     r5,4(r3)     u16 @4
//      stw     r4,16(r3)
//      mr      r5,r9
//      stw     r7,12(r3)
//      stw     r11,0(r3)
//      sth     r6,6(r3)
//      sth     r8,20(r3)
//      sth     r10,22(r3)
//      stw     r11,8(r3)
//      stw     r11,24(r3)
//      b       821558A8

struct Rec48
{
    /* 0x00 */ s32 f0;
    /* 0x04 */ u16 f4;
    /* 0x06 */ u16 f6;
    /* 0x08 */ s32 f8;
    /* 0x0C */ s32 f12;
    /* 0x10 */ s32 f16;
    /* 0x14 */ u16 f20;
    /* 0x16 */ u16 f22;
    /* 0x18 */ s32 f24;
};

ASSERT_OFFSET(Rec48, f16, 16);
ASSERT_OFFSET(Rec48, f22, 22);

void Tail_821558A8(Rec48*, void* a2, int a7, short a4, int a5,
                   short a6, int a8);

void Build48(Rec48* r, void* a2, short a3, short a4, int a5,
             short a6, int a7, short a8)
{
    r->f0  = 0;
    r->f4  = (unsigned short)a3;
    r->f16 = (s32)(int)a2;
    r->f12 = a5;
    r->f6  = (unsigned short)a4;
    r->f20 = (unsigned short)a6;
    r->f22 = (unsigned short)a8;
    r->f8  = 0;
    r->f24 = 0;
    Tail_821558A8(r, a2, a7, a4, a5, a6, (int)a8);
}

// NEAR-MISS. eight-arg form with r5<-r9 move; arg spellings differ.
