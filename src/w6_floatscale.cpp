#include "types.h"

// sub_821F7238 -- scale a float, truncate to int, index backwards from a
// field pointer. 44 B, 4 callers.
//
//      lis     r11,-32255
//      lwz     r10,72(r3)
//      lfs     f0,-22348(r11)   8201A8B4 -- 0xFFFFFFFF, i.e. -NaN
//      fmuls   f0,f1,f0
//      fctiwz  f13,f0
//      stfd    f13,-16(r1)
//      lwz     r9,-12(r1)       the low word of the converted int
//      rlwinm  r8,r9,2,0,29     * 4
//      subf    r7,r8,r10        base - 4*i
//      lfs     f1,0(r7)
//      blr
//
// The constant is NaN: float*NaN stays NaN through fmuls, and fctiwz of
// NaN is the most-negative integer, so the index is large and negative --
// a deliberate out-of-range probe. Spelled with the constant read from
// .rdata as an extern float; whether -NaN round-trips through a float
// literal is what this attempt measures.

extern const float kNaN_8201A8B4;

struct Scaled
{
    /* 0x48 */ char  unk0000[72];
    /* 0x48 */ float* end;
};

float Pick(Scaled* s, float t)
{
    int i = (int)(t * kNaN_8201A8B4);
    return s->end[i];
}

// NEAR-MISS. NaN constant semantics; index expression differs.
