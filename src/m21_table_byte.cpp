// sub_822D2550 -- read a BYTE field out of a global array element.
// 24 bytes, 4 callers.
//
//      lis     r11,-32099
//      mulli   r10,r3,1856        element stride
//      addi    r11,r11,-21312     \  the symbol's low half  -> 829CACC0
//      addi    r9,r11,1844        /  the FIELD offset, kept separate
//      lbzx    r3,r10,r9
//      blr
//
// Exactly the sibling of src/table_index.cpp (sub_822D2450): same base
// 829CACC0, same 1856-byte stride, and the same two consecutive `addi` --
// MSVC emits lis/addi as a relocated pair for the symbol and cannot fold a
// constant into the second half, so a field offset inside the element gets
// its own addi. There the field is an s32 at 1248 and the function returns
// its ADDRESS; here it is a byte at 1844 and the function returns its VALUE.
//
// `lbzx` and NOT a trailing `clrlwi r3,r3,24`: the return is u8, not bool.
// A bool return would have to normalise and the mask would be visible.
//
// The lis and the first addi are relocated, so 4 of 6 words are compared.

#include "types.h"

struct TEntry
{
    /* 0x0000 */ char unk0000[0x734];
    /* 0x0734 */ u8   kind;
    /* 0x0735 */ char unk0735[1856 - 0x734 - 1];
};

ASSERT_OFFSET(TEntry, kind, 0x734);
// Not a guess: `mulli r10,r3,1856` in the target states the stride, and
// table_index.cpp reaches the same number from the same base.
ASSERT_SIZE(TEntry, 1856);

extern TEntry g_entries[];

u8 KindOf(int i)
{
    return g_entries[i].kind;
}
