#include "types.h"

// sub_82151690 -- return the larger of two 16-bit fields. 44 B, 7 callers.
//
//      lwz     r11,64(r3)
//      lhz     r10,18(r11)
//      lhz     r9,16(r11)
//      extsh   r8,r10           <- only the +18 field is sign-extended
//      cmpw    cr6,r9,r8
//      ble-    cr6,lower
//      lhz     r3,16(r11)       reloaded, zero-extended
//      blr
// lower:lhz    r10,18(r11)      reloaded
//      extsh   r3,r10           sign-extended
//      blr
//
// The two fields have DIFFERENT signedness and that is readable off the
// instructions: +0x10 is loaded with `lhz` and never extended, so it is
// `unsigned short`; +0x12 gets an `extsh` every time it is used, so it is
// `short`. The comparison is `cmpw`, signed, on the widened pair -- which is
// what C's integer promotion does to a u16 and an s16.
//
// Both fields are RELOADED in their return paths rather than kept in a
// register, so neither was named in a local.
struct Extent
{
    char unk0000[16];
    u16  high;
    s16  low;
};
ASSERT_OFFSET(Extent, high, 16);
ASSERT_OFFSET(Extent, low, 18);

struct Owner40
{
    char    unk0000[64];
    Extent* extent;
};
ASSERT_OFFSET(Owner40, extent, 64);

int LargerOf(const Owner40* o)
{
    if (o->extent->high > o->extent->low)
        return o->extent->high;
    return o->extent->low;
}
