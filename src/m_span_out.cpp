#include "types.h"

// sub_82545348 -- write a span through an out-parameter. 48 B, 24 callers.
//
//      mr      r11,r3
//      cmplwi  cr6,r4,0
//      bne-    cr6,ok
//      li      r3,37            the same error code sub_8253A1C0 returns
//      blr
//  ok: lwz     r10,360(r11)     +0x168
//      li      r3,0
//      stw     r10,0(r4)        FIRST store
//      lwz     r9,372(r11)      +0x174
//      subf    r8,r9,r10        r10 - r9
//      stw     r8,0(r4)         SECOND store, same address
//      blr
//
// It stores through `out` TWICE, to the same address. That is not a
// transcription error: the source assigns and then adjusts, and the compiler
// keeps both stores because it cannot prove the first is dead -- `out` is a
// plain pointer, so the write is observable.
//
// 37 recurs as a null-argument error code (sub_8253A1C0 returns 37 for a
// null out-parameter and 36 for a null value), so these are one subsystem's
// error enum even though the two functions are 0x1B000 bytes apart.
struct Region
{
    char unk0000[0x168];
    s32  end;
    char unk016C[8];
    s32  start;
};
ASSERT_OFFSET(Region, end, 0x168);
ASSERT_OFFSET(Region, start, 0x174);

int GetSpan(const Region* r, int* out)
{
    if (!out)
        return 37;
    *out = r->end;
    *out = *out - r->start;
    return 0;
}
