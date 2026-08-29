// sub_826DB0A0 -- walk a pointer array backwards and squeeze out the null
// entries. 124 B, 4 callers.
//
// THIS IS sub_826C6298 AGAIN, WORD FOR WORD -- all 31 instructions, all the
// same registers, all the same offsets, at an address 60 KB away. Two
// separately emitted copies of one function body, which is what `/Gy` plus a
// template or an inlined-into-two-places helper leaves behind when the linker
// does not fold them (the two are in different regions of `.text`, so ICF
// never had them adjacent to consider).
//
// The reading is written out once, in src/n12_compact_nulls.cpp, and is not
// repeated here -- including why the u16 decrement comes out as an addition
// of a hoisted 0x0000FFFF rather than an `addi -1`, and why `items` and
// `count` are reloaded in both loops. The source is character for character
// the same, with the names changed so the two objects do not collide.
//
// Nothing is relocated; all 31 words are compared.

#include "types.h"

struct PtrVector2
{
    /* 0x00 */ void** items;
    /* 0x04 */ u16    count;
};
ASSERT_OFFSET(PtrVector2, count, 0x04);

void CompactNulls2(PtrVector2* o)
{
    for (int i = o->count - 1; i >= 0; i--)
    {
        if (o->items[i] == 0)
        {
            o->count = o->count - 1;
            for (int j = i; j < o->count; j++)
                o->items[j] = o->items[j + 1];
        }
    }
}
