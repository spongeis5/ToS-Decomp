#include "types.h"

// sub_82704688 -- pick one of two slots by a 1-based index. 40 B, 9 callers.
//
//      lwz     r11,40(r3)
//      addi    r10,r11,-1
//      cmplwi  cr6,r10,2       UNSIGNED compare of (i - 1) against 2
//      bge-    cr6,zero
//      addi    r11,r11,7
//      rlwinm  r11,r11,2,0,29
//      lwzx    r3,r11,r3       *(u32*)((char*)s + (i + 7) * 4)
//      blr
// zero:li      r3,0
//      blr
//
// One subtract and one UNSIGNED compare is a RANGE check written as such --
// `(u32)(i - 1) < 2` -- not `i == 1 || i == 2`, which would have emitted two
// compares and two branches the way m_state_2to4.cpp does.
//
// The index arithmetic reconciles: `entries` at 0x20 and `entries[i - 1]` is
// 0x20 + (i-1)*4 = (i + 7) * 4, which is what the `addi 7` and the shift
// build. Reading it as an array at offset 0 with a +7 bias would compile the
// same and be wrong about the type.
struct Selector
{
    char  unk0000[0x20];
    void* entries[2];
    u32   which;
};
ASSERT_OFFSET(Selector, entries, 0x20);
ASSERT_OFFSET(Selector, which, 0x28);

// NEEDS /O2 /Os, and for the reason that keeps recurring: at plain /O2 the
// shift goes to a fresh r10 and the indexed load reads it, where the target
// coalesces both onto r11. Two words, no source shape involved.
void* SelectEntry(const Selector* s)
{
    u32 i = s->which;
    if (i - 1 < 2)
        return s->entries[i - 1];
    return 0;
}
