// sub_8253A1C0 -- 44 bytes, 38 callers. Two null guards with distinct error
// codes, then a single store and success.
//
//      cmplwi  cr6,r4,0
//      bne-    cr6,+0x10
//      li      r3,37
//      blr
//      cmplwi  cr6,r3,0
//      bne-    cr6,+0x10
//      li      r3,36
//      blr
//      stw     r3,0(r4)
//      li      r3,0
//      blr
//
// The OUT pointer (r4, the second parameter) is tested FIRST and gets the
// larger code, 37 = 0x25; the value being handed back (r3, the first
// parameter) is tested second and gets 36 = 0x24. Ordinary `if (cond)
// return code;` shape: the bne- skips over the early return, so each return
// is the fall-through of its own compare and the guards are written in the
// order the compares appear.
//
// Nothing is relocated; all 11 words are compared.

#include "types.h"

int StoreOut(void* value, void** out)
{
    if (!out)
        return 37;
    if (!value)
        return 36;
    *out = value;
    return 0;
}
