#include "types.h"

// sub_821BFCA8 -- element address in a stride-52 array. 16 B, 3 callers.
//
//      lwz     r11,60(r3)       the array base, a field at +60
//      mulli   r10,r4,52        stride 52 -- not a power of two, so the
//                               constant IS the element size (idiom table)
//      add     r3,r10,r11       rA = the scaled index: the multiply's read
//                               comes after the base load (add-order lever)
//      blr
//
// Written as pointer arithmetic on a 52-byte element type; mulli by a small
// constant is also the /Os signature, so if /O2 composes the multiply out of
// shifts, the flag is the next thing to try, not the source.

struct Item52
{
    char unk0000[52];
};

ASSERT_SIZE(Item52, 52);

struct Holder
{
    /* 0x3C */ char    unk0000[60];
    /* 0x3C */ Item52* items;
};

ASSERT_OFFSET(Holder, items, 60);

Item52* At(Holder* h, int i)
{
    Item52* base = h->items;
    return (Item52*)(i * 52 + (char*)base);
}

// NEAR-MISS (2 of 4 words). Register-NUMBER allocation: the base lands in
// r10 and the mulli in r11; the image has r11/r10. The add's operand order
// is right, the assignment is not -- at /Os too, and across four spellings
// (pointer arith, char* + i*52, commuted, named local).
