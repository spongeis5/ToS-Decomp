#include "types.h"

// sub_822481B0 -- a global on/off flag guarding a lookup. 40 B, 25 callers.
//
//      lis     r11,0x8294
//      mr      r4,r3           key moves to arg 2 early
//      lbz     r10,-24616(r11) ; = 82939FD8, a byte
//      cmplwi  cr6,r10,0
//      beq-    cr6,tail
//      li      r3,0
//      blr
// tail:lis     r11,0x829A
//      lwz     r3,29924(r11)   ; = 829A74E4, a pointer
//      b       0x82247578
//
// BRANCH POLARITY: `beq-` jumps AWAY to the tail call, so the fall-through
// is the `return 0` and that path has to be written FIRST. Written the other
// way round -- `if (!g_disabled) return Lookup(...)` -- the compiler inverts
// the test and emits `bne-`, which is one word wrong and reads like a
// scheduling problem rather than a source-order one.
extern u8 g_lookupDisabled;
extern void* g_lookupContext;

void* LookupIn(void* ctx, void* key);

void* LookupKey(void* key)
{
    if (g_lookupDisabled)
        return 0;
    return LookupIn(g_lookupContext, key);
}
