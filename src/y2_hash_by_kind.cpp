// sub_821A9900 -- map a kind in 27..51 to one of eight 32-bit constants,
// with one shared fallback. 244 bytes, 2 callers, of which 100 are the jump
// table.
//
//      addi   r11,r3,-27
//      cmplwi cr6,r11,24 ; bgt- cr6,821A99E8      -> the fallback
//      lis/addi r12 = 821A9924 ; rlwinm r0,r11,2,0,29
//      lwzx   r0,r12,r0 ; mtctr r0 ; bctr
//
// The bias of 27 says the lowest case value is 27, and the 25 absolute
// entries at 821A9924 give the whole map:
//
//      27 28 32 35        -> A6F1954E
//      29 33 36           -> 8EF11AF2
//      31 34 37           -> BE49B97D
//      38                 -> 5466E841
//      40                 -> B11B8E5E
//      41                 -> 5621F781
//      49                 -> 81A27AAB
//      51                 -> B9B37786
//      30 39 42..48 50    -> 00130037, which is also the out-of-range target
//
// The blocks appear in the image in exactly that order with the fallback
// last, and MSVC lays case bodies out in SOURCE order and does not invent
// groups (MATCHED.md, measured on sub_827261D8), so the groups above are
// the groups as written and the fallback is `default:`.
//
// Every arm is `lis`/`ori`/`blr` -- three instructions, no shared tail --
// because the constants do not fit in 16 bits and there is nothing else to
// merge. The values look like hashes rather than flags: no two share a
// nibble pattern and none is a power of two.

#include "types.h"

u32 HashForKind(int kind)
{
    switch (kind)
    {
    case 27:
    case 28:
    case 32:
    case 35:
        return 0xA6F1954Eu;

    case 29:
    case 33:
    case 36:
        return 0x8EF11AF2u;

    case 31:
    case 34:
    case 37:
        return 0xBE49B97Du;

    case 38:
        return 0x5466E841u;

    case 40:
        return 0xB11B8E5Eu;

    case 41:
        return 0x5621F781u;

    case 49:
        return 0x81A27AABu;

    case 51:
        return 0xB9B37786u;

    default:
        return 0x00130037u;
    }
}
