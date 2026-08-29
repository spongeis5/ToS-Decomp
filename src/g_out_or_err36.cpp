#include "types.h"

// sub_8252BE30 -- two null checks with distinct error codes, then hand the
// first argument back through the out parameter. 44 B, 16 callers.
//
//      cmplwi  cr6,r3,0
//      bne-    cr6,0x8252BE40
//      li      r3,36
//      blr
//      cmplwi  cr6,r4,0
//      bne-    cr6,0x8252BE50
//      li      r3,37
//      blr
//      stw     r3,0(r4)
//      li      r3,0
//      blr
//
// Both `bne-` jump AWAY, so each error return is the FALL-THROUGH and the
// tests are written as the guards they look like. cmplwi is the unsigned
// compare a pointer gets. 36/37 are consecutive error codes.

s32 GetValue(void* value, void** out)
{
    if (value == 0)
        return 36;
    if (out == 0)
        return 37;

    *out = value;
    return 0;
}
