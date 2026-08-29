#include "types.h"

// sub_8224E7C0 -- element of a pointer array at +0. 16 B, 17 callers.
//   lwz r11,0(r3) ; rlwinm r10,r4,2,0,29 ; lwzx r3,r10,r11 ; blr
struct Arr { /* 0x00 */ void** items; };
ASSERT_OFFSET(Arr, items, 0x00);
void* ItemAt(Arr* a, int i) { return a->items[i]; }
