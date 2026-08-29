#include "types.h"

// sub_827245E0 -- the neighbour of sub_827245C0, doubling the base first.
// 32 B, 9 callers.
//   lwz r11,8(r3) ; lwz r10,0(r3) ; addi r9,r11,1
//   rlwinm r11,r9,1,0,30 ; add r8,r11,r4 ; rlwinm r7,r8,2,0,29
//   lwzx r3,r7,r10 ; blr
// (base + 1) * 2 + i, with the *2 as a shift.
struct Ring2 { void** items; char unk0004[0x04]; s32 base; };
ASSERT_OFFSET(Ring2, items, 0x00);
ASSERT_OFFSET(Ring2, base,  0x08);
void* RingAt2(Ring2* r, int i) { return r->items[(r->base + 1) * 2 + i]; }
