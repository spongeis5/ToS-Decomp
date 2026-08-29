// sub_82151670 -- "is the short field not -1". 28 B, 3 callers.
//
//      lwz   r11,64(r3)
//      lhz   r10,18(r11)
//      extsh r11,r10
//      addi  r11,r11,1
//      addic r9,r11,-1
//      subfe r3,r9,r11
//
// The same branchless `x != -1` as sub_821A2258, over a SIGNED 16-bit field:
// the value is loaded zero-extended and then `extsh`'d, which is what MSVC
// does for a `short` member, and the sign matters because -1 is the value
// being compared against.
//
// int return -- the 0/1 is produced directly by the subfe with no trailing
// clrlwi to normalise.

#include "types.h"

struct ShortHolder
{
    /* 0x00 */ char unk0000[0x12];
    /* 0x12 */ s16  id;
};
ASSERT_OFFSET(ShortHolder, id, 0x12);

struct ShortHolderOwner
{
    /* 0x00 */ char         unk0000[0x40];
    /* 0x40 */ ShortHolder* holder;
};
ASSERT_OFFSET(ShortHolderOwner, holder, 0x40);

int HasShortId(ShortHolderOwner* o)
{
    return o->holder->id != -1;
}
