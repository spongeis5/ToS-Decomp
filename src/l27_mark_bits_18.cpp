// sub_8224CD60 -- set a state byte and OR two bits into the next one.
// 24 B, 3 callers.
//
//      lbz r11,83(r3)
//      li  r10,4
//      ori r9,r11,24
//      stb r10,82(r3)
//      stb r9,83(r3)
//
// Two byte fields side by side: +82 is assigned 4, +83 gets 0x18 OR'd in.
// The load of +83 is emitted ahead of the store to +82, which needs no
// explanation beyond the scheduler -- two distinct constant offsets off one
// base provably do not alias, so MSVC is free to hoist it, and the store
// order 82 then 83 is the source order.
//
// `ori` on a value that arrived through `lbz` and leaves through `stb` is a
// byte field; nothing here masks, because the load already zero-extended and
// the store truncates.

#include "types.h"

struct MarkState
{
    /* 0x00 */ char unk0000[0x52];
    /* 0x52 */ u8   state;
    /* 0x53 */ u8   flags;
};
ASSERT_OFFSET(MarkState, state, 0x52);
ASSERT_OFFSET(MarkState, flags, 0x53);

void MarkState4(MarkState* m)
{
    m->state = 4;
    m->flags |= 0x18;
}
