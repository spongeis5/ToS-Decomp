#include "types.h"

// sub_821EE668 -- prepend a global and forward. 24 B, 8 callers.
//
//      mr      r11,r3
//      lis     r10,-32102
//      mr      r5,r4
//      addi    r3,r10,5072      ; = 829A13D0
//      mr      r4,r11
//      b       0x821EA7C0
//
// The three `mr`s are a rotation: the two incoming arguments move up one
// slot and the global takes the first. Nothing is loaded, so 829A13D0 is
// the object's ADDRESS, not a pointer read from there.
struct Registry;
extern Registry g_registry_829A13D0;

void Register(Registry* r, void* item, void* owner);

void RegisterDefault(void* item, void* owner)
{
    Register(&g_registry_829A13D0, item, owner);
}
