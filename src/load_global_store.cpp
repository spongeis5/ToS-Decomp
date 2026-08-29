#include "types.h"

// sub_8272CB68 -- copy a global pointer into a field. 16 B, 5 callers.
//   lis r11,-32104 ; lwz r11,10200(r11) ; stw r11,0(r3) ; blr
struct Slot { void* p; };
ASSERT_OFFSET(Slot, p, 0x00);
extern void* g_ptr_8272CB68;
void TakeGlobal(Slot* s) { s->p = g_ptr_8272CB68; }
