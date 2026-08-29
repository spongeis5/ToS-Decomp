// sub_8215A278 -- count the set bits of one global word. 56 bytes,
// 3 callers.
//
//      li     r10,32 ; li r3,0 ; li r11,1 ; mtctr r10
//      lis    r10,-32103 ; addi r9,r10,-18544     -> 8298B790
//      lwz    r10,9472(r9)                        loaded ONCE, before the loop
//  L:  and    r9,r11,r10
//      cmplwi cr6,r9,0 ; beq- cr6,<next>
//      addi   r3,r3,1
//  next:
//      rotlwi r11,r11,1
//      bdnz+  L
//      blr
//
// `li r10,32` then `mtctr` is a counted loop with 32 as the trip count, so
// the bound is a literal and not a width computed from anything.
//
// The mask is a strength-reduced `1 << i`: MSVC carries the value in r11 and
// advances it with `rotlwi`, not `slwi`, because the bit shifted off the top
// is dead on the last iteration and a rotate is the same instruction cost.
// That is the compiler's choice, so the source is an ordinary shift.
//
// The global word is hoisted out of the loop -- nothing in the body stores,
// so there is no aliasing to prevent it -- and the address is materialised
// with `lis`/`addi` and the field offset kept in the load, which per
// src/global_field.cpp is a member of a global OBJECT.
//
// 2 of 14 words are relocated.

#include "types.h"

struct BitGlobals
{
    /* 0x0000 */ u8  unk0000[9472];
    /* 0x2500 */ u32 mask;
};

ASSERT_OFFSET(BitGlobals, mask, 9472);

extern BitGlobals g_bitGlobals;

int CountSetBits()
{
    int n = 0;

    for (int i = 0; i < 32; i++)
    {
        if (g_bitGlobals.mask & (1u << i))
            n++;
    }

    return n;
}
