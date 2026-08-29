// sub_82726170 -- address of an element of a global array, stride 12.
// 28 bytes, 4 callers.
//
//      rlwinm  r11,r3,1,0,30      i*2
//      lis     r10,-32104
//      add     r11,r3,r11         i*3
//      addi    r10,r10,9272       -> 82982438
//      rlwinm  r11,r11,2,0,29     *4  -> i*12
//      add     r3,r11,r10
//      blr
//
// The stride is built as (i + i*2) * 4 rather than with a `mulli`, which is
// the shift/add form MSVC uses at /O2 for a small non-power-of-two element
// size -- the same idiom as src/stride24.cpp's (i + i*2) * 8. A `mulli 12`
// here would have been the /Os signature instead.
//
// The base arrives through a relocated lis/addi pair rather than a load, so
// this is a file-scope array and not a member: `&g_slots[i]`.
//
// 2 of 7 words are relocated.

#include "types.h"

struct Slot12
{
    /* 0x00 */ char unk0000[0x0C];
};

ASSERT_SIZE(Slot12, 12);

extern Slot12 g_slots[];

Slot12* SlotAt(int i)
{
    return &g_slots[i];
}
