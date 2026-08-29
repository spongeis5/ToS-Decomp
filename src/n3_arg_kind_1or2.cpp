// sub_822D3E60 -- "is the second argument's kind 1 or 2". 36 B, 4 callers.
//
//      lwz     r11,4(r4)        the SECOND argument, at +4
//      cmpwi   cr6,r11,1        SIGNED -- so the field is an int
//      beq-    cr6,set
//      cmpwi   cr6,r11,2
//      li      r11,0
//      bne-    cr6,out
// set: li      r11,1
// out: clrlwi  r3,r11,24
//      blr
//
// Instruction for instruction this is src/m_state_1or2.cpp (sub_821A5350)
// with the field read off r4 at +4 instead of off r3 at +0xD0, so the same
// reading applies: THE TRAILING `clrlwi r3,r11,24` IS `bool`, NOT `u8`.
// A u8, char or int return computes the 0/1 straight into r3 and stops one
// instruction shorter with no mask; only a bool return normalises, and the
// normalisation is what forces the value into r11 first.
//
// r3 IS NEVER READ. It is not a transcription slip -- nothing between the
// entry and the `clrlwi` touches it, and the `clrlwi` writes it. A first
// parameter that is passed and ignored is what `this` looks like in a
// predicate that only inspects its argument, so it is written as a member
// function; a free function with an unused first parameter compiles to the
// same eight instructions, and the bytes cannot tell them apart.
//
// Nothing is relocated; all 9 words are compared.

#include "types.h"

struct KindItem
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ s32  kind;
};
ASSERT_OFFSET(KindItem, kind, 0x04);

struct KindFilter
{
    char unk0000[0x04];

    bool Accepts(const KindItem* it) const;
};

bool KindFilter::Accepts(const KindItem* it) const
{
    return it->kind == 1 || it->kind == 2;
}
