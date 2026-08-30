#include "types.h"

// sub_825492D8 -- the twin of sub_825492B8 (src/m25_out_or_err37.cpp), 0x20
// later in the image and reading the field at 8 instead of the one at 4.
// 32 bytes.
//
//      cmplwi  cr6,r4,0
//      bne-    cr6,0x825492E8
//      li      r3,37
//      blr
//  825492E8:
//      lwz     r11,8(r3)
//      li      r3,0
//      stw     r11,0(r4)
//      blr
//
// Same reading as its twin: `cmplwi` is the unsigned null test this image
// always uses for a pointer, and the failing return being the FALL-THROUGH of
// an inverted test is what a leading guard produces -- `if (out == 0) return
// 37;` first, the success path second.  Written the other way round the
// polarity flips to `beq-`.
//
// `li r3,0` between the load and the store is scheduling, not source order:
// r3 is dead the instant the load retires.
//
// Nothing is relocated: 8 of 8 words are compared.

struct Holder
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ s32  value;
};

ASSERT_OFFSET(Holder, value, 0x08);

int FetchValue8(Holder* h, s32* out)
{
    if (out == 0)
        return 37;

    *out = h->value;
    return 0;
}
