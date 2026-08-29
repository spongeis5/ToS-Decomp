// sub_825492B8 -- write one field through an out-parameter, or return error
// 37 when the out-parameter is null. 32 bytes, 4 callers.
//
//      cmplwi  cr6,r4,0
//      bne-    cr6,0x825492C8
//      li      r3,37
//      blr
//  825492C8:
//      lwz     r11,4(r3)
//      li      r3,0
//      stw     r11,0(r4)
//      blr
//
// `cmplwi` and not `cmpwi`: an UNSIGNED null test, which in this image is
// always a pointer (a signed `cmpwi rX,0` on a value that is then
// dereferenced would mean an int holding a pointer -- see sub_82631D98).
//
// Branch polarity is source order. The failing return is the FALL-THROUGH of
// the inverted test, which is what a leading guard produces:
//
//      if (out == 0) return 37;
//
// Written the other way round -- `if (out) { ... return 0; } return 37;` --
// the success path becomes the fall-through and the polarity inverts to
// `beq-`, which is not what the image has.
//
// `li r3,0` lands between the load and the store because r3 is dead the
// instant the load retires and is then free to carry the return value; that
// is scheduling, not source order.
//
// Nothing is relocated: 8 of 8 words are compared.

#include "types.h"

struct Holder
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ s32  value;
};

ASSERT_OFFSET(Holder, value, 0x04);

int FetchValue(Holder* h, s32* out)
{
    if (out == 0)
        return 37;

    *out = h->value;
    return 0;
}
