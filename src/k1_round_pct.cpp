// sub_825CB000 -- scale a float by 100 and round half-away-from-zero to an
// int, with a sentinel result for anything at or below -60. 72 B, 5 callers.
//
//      lfs   f0,-5336(r11)      8205EB28 = -60.0f
//      fcmpu cr6,f1,f0
//      ble-  cr6,...            -> li r3,-10000 ; blr
//      lfs   f0,11760(r11)      82002DF0 =  0.5f
//      lfs   f13,12192(r10)     82002FA0 = -0.5f
//      fsel  f13,f1,f0,f13      f13 = (x >= 0.0f) ? 0.5f : -0.5f
//      lfs   f0,13364(r9)       82003434 = 100.0f
//      fmadds f0,f1,f0,f13      x * 100.0f + f13
//      fctiwz / stfd / lwz      the (int) cast
//
// The three separate `lis` of 0x82000000 are one per constant pool reference
// and carry no information; MSVC does not CSE the base.
//
// `ble-` jumping AWAY to the sentinel means the interesting path is the
// fall-through, so the `x > -60.0f` arm is written FIRST -- MATCHED.md,
// "branch polarity is source order".
//
// fsel frD,frA,frC,frB is `frD = (frA >= 0.0) ? frC : frB`, so frA is the
// tested value and the TRUE arm is the 0.5f. That fixes the ternary's
// polarity: `x >= 0.0f ? 0.5f : -0.5f`, not the inverse spelling.

#include "types.h"

s32 RoundPercent(f32 x)
{
    if (x > -60.0f)
        return (s32)(x * 100.0f + (x >= 0.0f ? 0.5f : -0.5f));
    return -10000;
}
