// sub_825B9970 -- zero three consecutive words. 20 B, 4 callers.
//
//   li  r11,0
//   stw r11,0(r3) ; stw r11,4(r3) ; stw r11,8(r3)
//   blr
//
// The same idiom as sub_8225FDD8 (src/zero3.cpp) at a different address, so
// this is a second, unrelated three-word record rather than the same one.
// Store order is address order, which is source order.

#include "types.h"

struct Triple3
{
    /* 0x00 */ s32 a;
    /* 0x04 */ s32 b;
    /* 0x08 */ s32 c;
};
ASSERT_OFFSET(Triple3, a, 0x00);
ASSERT_OFFSET(Triple3, b, 0x04);
ASSERT_OFFSET(Triple3, c, 0x08);

void ClearTriple3(Triple3* t)
{
    t->a = 0;
    t->b = 0;
    t->c = 0;
}
