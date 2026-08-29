// sub_8224E9F0 -- store two float arguments into two float globals.
// 20 bytes, 3 callers.
//
//      lis     r11,-32107
//      lis     r10,-32107
//      stfs    f1,25380(r11)       -> 82956324
//      stfs    f2,25384(r10)       -> 82956328
//      blr
//
// TWO `lis` for two ADJACENT addresses is the whole content of this function.
// Four bytes apart is well inside a displacement, so a single struct would
// have formed the base once and put 0 and 4 in the stores. A separate
// relocated high half per access is what MSVC emits for two DISTINCT symbols,
// because each one's `lis`/`stfs` pair is its own relocation and neither half
// can be shared.
//
// f1 and f2 are the first two float argument registers, and no GPR is
// touched, so both parameters are floats.
//
// The two `lis` and the two `stfs` are all relocated, which leaves only the
// `blr` -- match.py refuses a match where every compared word was relocated,
// and here 1 of 5 survives.

#include "types.h"

extern float g_paramA;
extern float g_paramB;

void SetParams(float a, float b)
{
    g_paramA = a;
    g_paramB = b;
}
