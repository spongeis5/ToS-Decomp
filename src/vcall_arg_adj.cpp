#include "types.h"

// sub_828864E0 -- virtual call with the second argument adjusted.
// 20 B, 16 callers.
//   lwz r11,0(r3) ; addi r4,r4,120 ; lwz r11,44(r11) ; mtctr r11 ; bctr
// Slot 44/4 = 11. The free-function form put the slot in r10; the target
// reuses r11.
struct T11;
struct VT11 { void* (*slot[12])(T11*, void*); };
struct T11
{
    VT11* vt;
    void* Call(char* p);
};
ASSERT_OFFSET(T11, vt, 0x00);

void* T11::Call(char* p) { return vt->slot[11](this, p + 120); }
