// sub_82698EE8 -- look a two-byte key up in a short table of 8-byte entries;
// on a miss return a base plus the key's second byte. 104 B, 5 callers.
//
//   lwz r9,88(r3) ; li r10,0 ; cmpwi cr6,r9,0 ; ble- <tail>
//
// The count is `int` (`cmpwi`, and the bottom test is `cmpw`), and the
// zero-trip test in front of the loop with the increment/compare at the
// bottom is MSVC's ordinary `for` rotation. The count is loaded ONCE and kept
// in r9 across the loop -- there is no store in the body, so it hoists on its
// own and does not need naming in a local.
//
//   addi r11,r3,76 ; lwz r7,-4(r11) ... lbz r5,0(r11) ; addi r11,r11,8
//
// is the induction pointer, biased so both fields of the element at +72+8i
// reach with a small displacement. The FOUND block re-derives the address
// from the index (`rlwinm r11,r10,3,0,28 ; add ; lbz r3,77(r11)`), which is
// what an index-written loop gives when the return is laid out of line.
//
// `lwz` + `clrlwi ...,24` on the first field, rather than `lbz`, says the
// field is a WORD and the source narrows it: `(u8)e->code`.
//
// LAYOUT NOTE, stated because it constrains the struct rather than because it
// is comfortable: the entry array starts at +72 and `count` is at +88, so the
// array can hold exactly TWO entries before it would run into the count. The
// bytes do not say more than that, and a longer array would be inventing a
// layout the image contradicts.
//
// The return is NOT masked on either path, so the return type is `int`: a
// `u8` return would have to truncate `t->base + k->b` and does not.
// `add r3,r11,r10` puts the key byte in rA, and by the `add`-operand-order
// lever that is the operand whose source read comes LATER -- hence
// `t->base + k->b` and not the other way round.

#include "types.h"

struct Key
{
    /* 0x00 */ u8 a;
    /* 0x01 */ u8 b;
};

struct Entry
{
    /* 0x00 */ u32 code;
    /* 0x04 */ u8  sel;
    /* 0x05 */ u8  value;
    /* 0x06 */ u8  unk0006[2];
};

ASSERT_SIZE(Entry, 8);

struct Table
{
    /* 0x00 */ u8    unk0000[0x40];
    /* 0x40 */ int   base;
    /* 0x44 */ u8    unk0044[4];
    /* 0x48 */ Entry entries[2];
    /* 0x58 */ int   count;
};

ASSERT_OFFSET(Table, base, 0x40);
ASSERT_OFFSET(Table, entries, 0x48);
ASSERT_OFFSET(Table, count, 0x58);

int PairLookup(Table* t, const Key* k)
{
    int i;

    for (i = 0; i < t->count; i++)
    {
        if (k->b == (u8)t->entries[i].code && k->a == t->entries[i].sel)
            return t->entries[i].value;
    }

    return t->base + k->b;
}
