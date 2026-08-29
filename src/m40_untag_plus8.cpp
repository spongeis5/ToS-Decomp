// sub_826A7950 -- strip two tag bits off a stored word and step past an
// 8-byte header. 16 bytes, 3 callers.
//
//      lwz     r11,36(r3)
//      rlwinm  r11,r11,0,0,29      keep bits 0..29 -> & ~3
//      addi    r3,r11,8
//      blr
//
// `rlwinm rD,rS,0,0,29` is a mask with no rotate, and the mask is 0xFFFFFFFC,
// so the low two bits of the stored word carry a tag. There is no null test
// anywhere, so this is not an upcast -- the adjustment is unconditional.

#include "types.h"

struct Tagged
{
    /* 0x00 */ u8  unk0000[0x24];
    /* 0x24 */ u32 link;
};

ASSERT_OFFSET(Tagged, link, 0x24);

void* PayloadOf(Tagged* t)
{
    return (char*)(t->link & ~3) + 8;
}
