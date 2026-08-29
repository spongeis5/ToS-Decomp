#include "types.h"

// sub_825E3598 -- the neighbour of sub_825E35C8, slot 8/4 = 2.
// 24 B, 5 callers.
//   lis r11,-32093 ; lwz r3,18444(r11) ; lwz r11,0(r3)
//   lwz r11,8(r11) ; mtctr r11 ; bctr
// Same global, 48 bytes earlier -- almost certainly the same translation
// unit.
struct GT2;
struct GVT2 { void* (*slot[3])(GT2*); };
struct GT2 { GVT2* vt; };
ASSERT_OFFSET(GT2, vt, 0x00);
extern GT2* g_singleton_825E3598;
void* CallGlobal2()
{
    GT2* g = g_singleton_825E3598;
    return g->vt->slot[2](g);
}
