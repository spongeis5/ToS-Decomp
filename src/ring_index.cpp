#include "types.h"

// sub_827245C0 -- index an array by a stored base plus an argument.
// 28 B, 5 callers.
//   lwz r11,8(r3) ; lwz r10,0(r3) ; add r11,r11,r4 ; addi r9,r11,1
//   rlwinm r8,r9,2,0,29 ; lwzx r3,r8,r10 ; blr
struct Ring { void** items; char unk0004[0x04]; s32 base; };
ASSERT_OFFSET(Ring, items, 0x00);
ASSERT_OFFSET(Ring, base,  0x08);
void* RingAt(Ring* r, int i) { return r->items[r->base + i + 1]; }
