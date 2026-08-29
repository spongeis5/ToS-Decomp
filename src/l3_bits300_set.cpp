// sub_825DB730 -- "is either of two flag bits set". 20 B, 4 callers.
//
//   lwz    r11,20(r3)
//   rlwinm r11,r11,0,22,23    rotate 0, keep bits 22..23 -> mask 0x00000300
//   addic  r10,r11,-1
//   subfe  r3,r10,r11         branchless `!= 0`
//
// `addic rD,rS,-1 ; subfe rT,rD,rS` is the branchless `x != 0` from the
// idiom table: rS == 0 gives -1 with no carry and subfe yields 0; rS != 0
// carries and subfe yields 1.
//
// Bit numbering is big-endian, so MB=22 is 1 << (31-22) = 0x200 and ME=23 is
// 0x100.  The mask is 0x300 and it is the whole of what is tested.
//
// The 0/1 lands directly in r3 with no trailing clrlwi -- the `int` shape,
// as in src/m_ready_not255.cpp which ends in the same two instructions.
//
// /Os DECIDED THIS ONE, by the documented signature and nothing else.  At
// /O2 the instructions and their order are already right and three words
// differ only in register NAME: the mask goes to a fresh r10 and the addic
// to a fresh r9, where the target reuses r11 and r10.  2 of 5 at /O2,
// 5 of 5 at /O2 /Os with the source untouched.

#include "types.h"

struct Bits300
{
    /* 0x00 */ char unk0000[0x14];
    /* 0x14 */ u32  flags;
};
ASSERT_OFFSET(Bits300, flags, 0x14);

int HasEitherBit(Bits300* p)
{
    return (p->flags & 0x300) != 0;
}
