// sub_825B9E20 -- "are the low three flag bits all clear". 20 B, 4 callers.
//
//   lwz    r11,20(r3)
//   clrlwi r10,r11,29        r11 & 7
//   cntlzw r9,r10
//   rlwinm r3,r9,27,31,31    bit 5 of the count: 1 exactly when the AND is 0
//
// `cntlzw ; rlwinm rX,rY,27,31,31` with NO `addi` ahead of it is the
// branchless `x == 0` from the idiom table.  The mask is spelled by the
// clrlwi's rotate-0/ME-29 form, so it is the low three bits and nothing
// else.
//
// The 0/1 is materialised straight into r3 by the rlwinm and there is no
// trailing clrlwi, which is the `int` shape (a bool return would have to
// normalise from a scratch).  Matches src/eq2_208.cpp and src/eq1_144_36.cpp,
// both `int`.

#include "types.h"

struct Low3
{
    /* 0x00 */ char unk0000[0x14];
    /* 0x14 */ u32  flags;
};
ASSERT_OFFSET(Low3, flags, 0x14);

int HasNoLow3(Low3* p)
{
    return (p->flags & 7) == 0;
}
