// sub_826B26A8 -- read a 12-bit field and return it as a float, scaled by
// 1/16 when a flag bit is set. 76 B, 5 callers.
//
//      lhz     r11,6(r3)          the FLAG halfword, loaded first
//      lhz     r10,4(r3)          the VALUE halfword
//      rlwinm. r9,r11,0,27,27     & 0x10, no rotate -> an inline mask test
//      clrlwi  r11,r10,20         & 0x0FFF
//      clrldi  r11,r11,32         zero-extend: the value is UNSIGNED
//      bne-    ...                the scaled arm is out of line
//      std/lfd/fcfid/frsp         (f32) of a u32
//      ...
//      lfs     f0,12880(r10)      82003250 = 0.0625f
//
// The rotate amount is ZERO on the flag test, which per MATCHED.md is an
// inline mask rather than a bool-returning accessor.
//
// The masked value is computed ONCE, above the branch, but the int-to-float
// conversion is emitted TWICE -- once per arm. That is the shape of the same
// masked sub-expression appearing inside two separate return statements, so
// the cast is not hoisted into a local.
//
// The flag halfword is loaded before the value halfword, which is why the
// test is written first even though the value is what both arms return.

#include "types.h"

struct PackedField
{
    /* 0x00 */ u8  unk0000[0x04];
    /* 0x04 */ u16 value;
    /* 0x06 */ u16 flags;
};
ASSERT_OFFSET(PackedField, value, 0x04);
ASSERT_OFFSET(PackedField, flags, 0x06);

f32 FieldScaled(const PackedField* p)
{
    u32 v = p->value & 0xFFF;

    if ((p->flags & 0x10) == 0)
        return (f32)v;
    return (f32)v * 0.0625f;
}
