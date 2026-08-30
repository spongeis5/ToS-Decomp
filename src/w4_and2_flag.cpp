#include "types.h"

// sub_82761AA8 -- guard on a 1-bit flag; if clear, set two bits. 40 B, 4
// callers.
//
//      lbz     r11,333(r3)
//      clrlwi. r10,r11,31       test the byte's LOW bit, record form -- the
//                               dot is an /Os property (MATCHED.md, 827156B8)
//      bnelr                    guard as a conditional RETURN, body is the
//                               fall-through (idiom table)
//      lbz     r10,335(r3)
//      li      r9,1
//      ori     r10,r10,2
//      rlwimi  r11,r9,0,31,23   insert 1 with no shift; the mask reaches
//                               past the byte, so the stored byte gets its
//                               low bit SET and nothing else preserved --
//                               a bitfield ASSIGNMENT, not an |=
//      stb     r10,335(r3)      byte 335 first
//      stb     r11,333(r3)
//      blr
//
// The rlwimi mask (31..23, SH=0) is what `flags = 1` on a 1-bit field
// produces; the `|= 0x80` spelling was measured and gives rlwimi
// SH=7, MB=0, ME=24 instead -- one word wrong either way until spelled as
// the bitfield the mask implies. MSB-first bitfield allocation is measured
// (MATCHED.md), so a :1 field declared after :7 sits at the byte's low bit.

struct Flagged
{
    /* 0x14D */ char unk0000[333];
    /* 0x14D */ u8   spare : 7;
    /* 0x14D */ u8   flags : 1;
    /* 0x14E */ char unk014E;
    /* 0x14F */ u8   mask;
};

void TrySet(Flagged* f)
{
    if (f->flags)
        return;
    f->mask  |= 2;
    f->flags = 1;
}
