#include "types.h"

// sub_82203890 -- read bit 0 of a field of the object held at +0. 16 B, 4 callers.
//
//      lwz     r11,0(r3)
//      lwz     r10,56(r11)
//      clrlwi  r3,r10,31
//      blr
//
// Two indirections: the argument holds a pointer at +0x00, and the flag word
// is at +0x38 of THAT object.
//
// `clrlwi r3,r10,31` is `rlwinm r3,r10,0,31,31` -- rotate ZERO, bit already
// at 31. MATCHED.md's bit lever reads the rotate amount to tell an inline
// mask from a bool-returning accessor, and for bit 0 the two coincide: no
// rotate is needed either way, so the encoding does NOT decide between
// `bool` and `u32 & 1` here. It is written `bool` because the value lands
// straight in r3 with nothing further done to it, which is what a normalised
// return looks like; the bytes do not assert it.

struct Flags38
{
    /* 0x00 */ char unk0000[0x38];
    /* 0x38 */ u32  flags;
};
ASSERT_OFFSET(Flags38, flags, 0x38);

struct HolderBit0
{
    /* 0x00 */ Flags38* held;
};
ASSERT_OFFSET(HolderBit0, held, 0x00);

bool IsBit0Set(HolderBit0* h)
{
    return (h->held->flags & 1) != 0;
}
