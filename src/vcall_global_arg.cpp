#include "types.h"

// sub_826C5E00 -- virtual call on a global, forwarding the argument.
// 28 B, 19 callers.
//   lis r11,-32091 ; mr r4,r3 ; lwz r3,-21512(r11)
//   lwz r10,0(r3) ; lwz r9,60(r10) ; mtctr r9 ; bctr
// Slot 60/4 = 15. The vtable and slot go to FRESH registers here (r10, r9).
struct GT;
struct GVT { void* (*slot[16])(GT*, void*); };
struct GT { GVT* vt; };
ASSERT_OFFSET(GT, vt, 0x00);
extern GT* g_singleton_826C5E00;
void* CallGlobal15(void* p)
{
    GT* g = g_singleton_826C5E00;
    return g->vt->slot[15](g, p);
}
