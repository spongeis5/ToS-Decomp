// sub_825FB3E0 -- the 15-cell twin of sub_821581C0 (see a1_grid_range16.cpp).
// 180 B, 6 callers.  Identical instruction for instruction except that the
// two .rdata multipliers are +15.0f (8200C688) and -15.0f (8200CCD8) and the
// upper clamp is 15 instead of 16.
//
//   lis r11,-32255 ; lfs f0,-14712(r11)   -> 8200C688 =  15.0f
//   lis r11,-32255 ; lfs f13,-13096(r11)  -> 8200CCD8 = -15.0f
//   li  r10,15                            the shared upper clamp
//   cmpwi cr6,r11,15 ; ble- / blelr
//
// The two constants are far apart in .rdata, so they are not a pair the
// linker folded -- they are two separate literals in the same translation
// unit, exactly as in the 16 version.

#include "types.h"

void GridRange15(int* x0, int* x1, int* y0, int* y1, float cx, float cy, float r)
{
    int v;

    v = (int)((cx - r) * 15.0f);
    *x0 = v;
    if (v < 0)
        *x0 = 0;

    v = 2 - (int)((cx + r) * -15.0f);
    *x1 = v;
    if (v > 15)
        *x1 = 15;

    v = (int)((cy - r) * 15.0f);
    *y0 = v;
    if (v < 0)
        *y0 = 0;

    v = 2 - (int)((cy + r) * -15.0f);
    *y1 = v;
    if (v > 15)
        *y1 = 15;
}
