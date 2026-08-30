// sub_82524E90 -- fill up to six optional out-parameters from one object,
// the last of them through a switch on the mode. 256 bytes, 1 caller, of
// which 44 are the jump table.
//
// Five identical guarded copies:
//
//      cmplwi cr6,r4,0 ; beq- ; lwz r11,1084(r3) ; stw r11,0(r4)
//      cmplwi cr6,r5,0 ; beq- ; lwz r11,1080(r3) ; stw r11,0(r5)
//      cmplwi cr6,r6,0 ; beq- ; lwz r11,1096(r3) ; stw r11,0(r6)
//      cmplwi cr6,r7,0 ; beq- ; lwz r11,1092(r3) ; stw r11,0(r7)
//      cmplwi cr6,r8,0 ; beq- ; lis r11,0 ; ori r10,r11,38228
//                               lwzx r7,r3,r10 ; stw r7,0(r8)
//
// The fifth needs `lis`/`ori` and an INDEXED load because 38228 = 0x9554 is
// past the signed 16-bit displacement, which is what makes it evidence: the
// field really is at 0x9554 of one object and not a second pointer.
//
// `lwz r11,1080(r3)` appears TWICE -- once for the second out-parameter and
// once as the switch value -- with no CSE, because the stores in between are
// through pointers MSVC cannot disambiguate from the object.
//
// The dispatch is `cmplwi cr6,r11,10 ; bgt-` with NO `addi r11,r11,-1`, so
// the table is unbiased and case 0 is written out; per
// src/y2_rate_table.cpp that also means case 0 does NOT share `default:`'s
// label. Here it does not: 0 shares a block with 6..10, and the default --
// anything above 10 -- stores nothing at all and falls straight to the
// shared `li r3,0 ; blr`.
//
// Table entries, and so the case groups:
//
//      0 -> 82524F80   1 -> 82524F40   2 -> 82524F50   3 -> 82524F60
//      4 -> 82524F70   5 -> 82524F70   6..10 -> 82524F80
//
// and the blocks appear in the image as 8, 16, 24, 32, 0. MSVC lays case
// bodies out in source order, so that is the order they are written, with
// the {4,5} and {0,6,7,8,9,10} groups written as groups.
//
// `cmplwi` on the mode makes it unsigned; every arm materialises its own
// `li r3,0` because the shared return is one instruction and gets
// duplicated into each tail.

#include "types.h"

struct QueryObj
{
    /* 0x0000 */ u8  unk0000[0x438];
    /* 0x0438 */ u32 mode;
    /* 0x043C */ u32 f043C;
    /* 0x0440 */ u8  unk0440[0x04];
    /* 0x0444 */ u32 f0444;
    /* 0x0448 */ u32 f0448;
    /* 0x044C */ u8  unk044C[0x9554 - 0x44C];
    /* 0x9554 */ u32 f9554;
};
ASSERT_OFFSET(QueryObj, mode,  0x0438);
ASSERT_OFFSET(QueryObj, f043C, 0x043C);
ASSERT_OFFSET(QueryObj, f0444, 0x0444);
ASSERT_OFFSET(QueryObj, f0448, 0x0448);
ASSERT_OFFSET(QueryObj, f9554, 0x9554);

int QueryInfo(QueryObj* o, u32* a, u32* b, u32* c, u32* d, u32* e, u32* bits)
{
    if (a != 0)
        *a = o->f043C;
    if (b != 0)
        *b = o->mode;
    if (c != 0)
        *c = o->f0448;
    if (d != 0)
        *d = o->f0444;
    if (e != 0)
        *e = o->f9554;

    if (bits != 0)
    {
        switch (o->mode)
        {
        case 1:
            *bits = 8;
            break;
        case 2:
            *bits = 16;
            break;
        case 3:
            *bits = 24;
            break;
        case 4:
        case 5:
            *bits = 32;
            break;
        case 0:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
            *bits = 0;
            break;
        }
    }

    return 0;
}
