#include "types.h"

// sub_821A6260 -- write a five-float constant set through an out-parameter
// that arrives in r4: the function carries an UNUSED LEADING PARAMETER in
// r3, the same shape as the FMOD mdct_init functions. 48 B, 3 callers.
//
//      addi    r9,r11,15368     = 82003C08
//      lfs     f12,15368(r11)   0.2617993950843811   (pi/12)
//      lfs     f0,11684(r10)    0.0                  82002DA4
//      lfs     f13,-4(r9)       9.0                  82003C04
//      stfs    f13,0(r4)
//      stfs    f12,4(r4)
//      stfs    f0,12(r4)        <- 12 before 8
//      stfs    f0,8(r4)
//      stfs    f0,16(r4)
//      blr
//
// Store order is source order (MATCHED.md), including when it is not address
// order, so the source writes +12 ahead of +8. The zero at 82002DA4 is loaded
// once and stored three times -- one literal spelled four times, CSE'd.

struct Vec5
{
    float f0;
    float f4;
    float f8;
    float f12;
    float f16;
};

static const float kInit[2] = { 9.0f, 0.2617993950843811f };

void Init5(void* self, Vec5* v)
{
    v->f0  = kInit[0];
    v->f4  = kInit[1];
    v->f12 = 0.0f;
    v->f8  = 0.0f;
    v->f16 = 0.0f;
}

// NEAR-MISS (1 of 12 words after the array spelling). The image forms
// pi/12's address as a lis+addi PAIR (lis r11 ; addi r9,r11,15368) and
// reads 9.0 as -4(r9): the CSE anchor is the SECOND array element. Every
// spelling tried anchors element 0 and reads element 1 as +4(r9), with
// f12/f13 swapped. CAUGHT BY build.py, not match.py: the literal spelling
// passed match.py while holding a lis where the image has an addi, and
// the resolution cannot agree. Second recorded instance of the
// register-inside-a-relocated-word class the build is stricter about.
