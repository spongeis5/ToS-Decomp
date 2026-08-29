#include "types.h"

// sub_825E35C8 -- virtual call on a global, no arguments. 24 B, 5 callers.
//   lis r11,-32093 ; lwz r3,18444(r11) ; lwz r11,0(r3)
//   lwz r11,16(r11) ; mtctr r11 ; bctr
// Slot 16/4 = 4. Here the target REUSES r11 for the slot.
struct GT4;
struct GVT4 { void* (*slot[5])(GT4*); };
struct GT4 { GVT4* vt; };
ASSERT_OFFSET(GT4, vt, 0x00);
extern GT4* g_singleton_825E35C8;
void* CallGlobal4()
{
    GT4* g = g_singleton_825E35C8;
    return g->vt->slot[4](g);
}
