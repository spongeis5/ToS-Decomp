// sub_821581C0 -- turn a circle (centre, radius) into an inclusive cell range
// on a 16-cell grid, four `int*` out-parameters. 180 B, 6 callers.
//
//   fsubs f13,f1,f3 ; lfs f0,12288(r11)   -> 82003000 = 16.0f
//   fmuls f12,f13,f0 ; fctiwz ; stfd -16(r1) ; lwz r10,-12(r1)
//   cmpwi cr6,r10,0 ; stw r10,0(r3) ; bge- ; stw r9,0(r3)      r9 = 0
//   fadds f12,f1,f3 ; lfs f13,12284(r11)  -> 82002FFC = -16.0f
//   fmuls f11,f12,f13 ; fctiwz ; ... ; subfic r11,r8,2
//   stw r11,0(r4) ; cmpwi cr6,r11,16 ; ble- ; stw r10,0(r4)    r10 = 16
//   ... the same two blocks again on f2, into r5 and r6, the last clamp
//   written as `blelr` because there is nothing after it.
//
// The two multipliers are SEPARATE .rdata constants, +16.0f and -16.0f, and
// each is loaded once and reused by the matching block of the second axis --
// f0 survives from block 1 into block 3, f13 from block 2 into block 4.
// So the source really does multiply by a negative constant rather than
// negating; `2 - (int)(v * -16.0f)` is `2 + ceil(v * 16.0f)`, the upper
// bound of the span.
//
// `li r9,0` is hoisted to the top and `li r10,16` into the second block:
// each clamp constant is materialised once and shared by both axes.
//
// The value is stored and then OVERWRITTEN on the clamp path -- store, test
// the register, maybe store again -- so the clamp reads the local, not the
// memory it just wrote.
//
// sub_825FB3E0 is the same function over a 15-cell grid; see a2.

#include "types.h"

void GridRange16(int* x0, int* x1, int* y0, int* y1, float cx, float cy, float r)
{
    int v;

    v = (int)((cx - r) * 16.0f);
    *x0 = v;
    if (v < 0)
        *x0 = 0;

    v = 2 - (int)((cx + r) * -16.0f);
    *x1 = v;
    if (v > 16)
        *x1 = 16;

    v = (int)((cy - r) * 16.0f);
    *y0 = v;
    if (v < 0)
        *y0 = 0;

    v = 2 - (int)((cy + r) * -16.0f);
    *y1 = v;
    if (v > 16)
        *y1 = 16;
}
