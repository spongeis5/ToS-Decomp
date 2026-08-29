// sub_8224EA18 -- hand two float constants back through two out-parameters.
// 28 bytes, 3 callers.
//
//      lis     r11,-32255
//      lis     r10,-32255
//      lfs     f0,-11144(r11)      -> 8200D478, which holds 5.0f
//      lfs     f13,-11140(r10)     -> 8200D47C, which holds 30.0f
//      stfs    f0,0(r3)
//      stfs    f13,0(r4)
//      blr
//
// Two `lis` four bytes apart, the same shape as its neighbour
// src/m44_store_two_globals.cpp: each float literal is its own constant-pool
// symbol with its own relocated high half, and neither half can be shared.
// A single array or struct would have formed the base once.
//
// Both stores are at offset 0 of DIFFERENT pointers, so these are two
// out-parameters and not two fields.
//
// The values were read out of the image, not inferred: 5.0f and 30.0f.
//
// 4 of 7 words are relocated.

#include "types.h"

void GetRange(float* lo, float* hi)
{
    *lo = 5.0f;
    *hi = 30.0f;
}
