// sub_82261D20 -- clear a word and set a flag bit. 24 B, 3 callers.
//
//      lbz r11,64(r3)
//      li  r10,0
//      ori r9,r11,64
//      stw r10,272(r3)
//      stb r9,64(r3)
//
// The byte at +64 is read before the word at +272 is stored and written
// after it: distinct constant offsets off one base cannot alias, so the load
// is hoisted freely and the two stores keep their source order, 272 then 64.
//
// 64 as a bit is 0x40, one bit of the flags byte, OR'd in without a mask
// because `lbz` already zero-extended it.

#include "types.h"

struct MarkedRecord
{
    /* 0x0000 */ char unk0000[0x40];
    /* 0x0040 */ u8   flags;
    /* 0x0041 */ char unk0041[0xCF];
    /* 0x0110 */ s32  count;
};
ASSERT_OFFSET(MarkedRecord, flags, 0x40);
ASSERT_OFFSET(MarkedRecord, count, 0x110);

void ClearAndMark(MarkedRecord* r)
{
    r->count = 0;
    r->flags |= 0x40;
}
