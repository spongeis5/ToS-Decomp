// sub_8283F298 -- zero the halfword of eight 4-byte slots, then three bytes
// after them. 52 bytes, 3 callers.
//
//      li  r11,0
//      sth r11,2(r3) ; sth r11,6(r3) ; ... ; sth r11,30(r3)
//      stb r11,33(r3) ; stb r11,32(r3) ; stb r11,34(r3)
//      blr
//
// Eight `sth` at 2, 6, 10 ... 30 -- stride 4 with the halfword at +2 of each
// element, so the slot is four bytes and its second half is what gets
// cleared. Fully UNROLLED with no `mtctr`/`bdnz`, so these are eight separate
// statements: MSVC does not unroll a counted loop into eight stores here, and
// a loop would have left its counter behind.
//
// The three bytes come out 33, 32, 34 -- not address order -- and store order
// is source order, so that is how they were written.
//
// Nothing is relocated: 13 of 13 words are compared.

#include "types.h"

struct Slot4
{
    /* 0x00 */ u8  unk0000[2];
    /* 0x02 */ u16 value;
};

ASSERT_SIZE(Slot4, 4);

struct SlotSet
{
    /* 0x00 */ Slot4 slots[8];
    /* 0x20 */ u8    a;
    /* 0x21 */ u8    b;
    /* 0x22 */ u8    c;
};

ASSERT_OFFSET(SlotSet, a, 32);
ASSERT_OFFSET(SlotSet, c, 34);

void ClearSlots(SlotSet* s)
{
    s->slots[0].value = 0;
    s->slots[1].value = 0;
    s->slots[2].value = 0;
    s->slots[3].value = 0;
    s->slots[4].value = 0;
    s->slots[5].value = 0;
    s->slots[6].value = 0;
    s->slots[7].value = 0;

    s->b = 0;
    s->a = 0;
    s->c = 0;
}
