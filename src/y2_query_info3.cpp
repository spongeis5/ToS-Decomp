// sub_82583860 -- fill up to four optional out-parameters from one object,
// the last through a switch on the mode. 216 bytes, no direct callers (it is
// reached through a vtable), of which 44 are the jump table.
//
// It is src/y2_query_info.cpp (sub_82524E90) with three copies instead of
// five and a much smaller object: the fields are at 20, 24 and 68, and the
// switch reads 24 a SECOND time, exactly as the other one re-reads its mode.
// The two share the same switch, arm for arm:
//
//      0 -> 0    1 -> 8    2 -> 16    3 -> 24    4,5 -> 32    6..10 -> 0
//
// with `cmplwi cr6,r11,10 ; bgt-` and no `addi r11,r11,-1`, so case 0 is
// written out and is NOT sharing `default:`'s label -- it is grouped with
// 6..10, while the default stores nothing and falls to the shared
// `li r3,0 ; blr`. Block order in the image is 8, 16, 24, 32, 0, which is
// source order.
//
// Two functions this similar, 0x5F000 bytes apart, are the same routine for
// two different objects rather than one shared helper.

#include "types.h"

struct QueryObj3
{
    /* 0x00 */ u8  unk0000[0x14];
    /* 0x14 */ u32 f0014;
    /* 0x18 */ u32 mode;
    /* 0x1C */ u8  unk001C[0x28];
    /* 0x44 */ u32 f0044;
};
ASSERT_OFFSET(QueryObj3, f0014, 0x14);
ASSERT_OFFSET(QueryObj3, mode,  0x18);
ASSERT_OFFSET(QueryObj3, f0044, 0x44);

int QueryInfo3(QueryObj3* o, u32* a, u32* b, u32* c, u32* bits)
{
    if (a != 0)
        *a = o->f0014;
    if (b != 0)
        *b = o->mode;
    if (c != 0)
        *c = o->f0044;

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
