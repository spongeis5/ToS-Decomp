#include "types.h"

// sub_82727258 -- address of an 8-byte element. 16 B, 7 callers.
//   lwz r11,0(r3) ; rlwinm r10,r4,3,0,28 ; add r3,r10,r11 ; blr
struct E8 { char unk0000[8]; };
ASSERT_SIZE(E8, 8);
struct Holder8 { E8* items; };
ASSERT_OFFSET(Holder8, items, 0x00);
E8* At8(Holder8* h, int i) { return &h->items[i]; }
