#include "types.h"

// sub_82250B88 -- test a field of an array element for zero. 24 B, 5 callers.
//   rlwinm r11,r4,4,0,27 ; add r11,r11,r3 ; lwz r10,12(r11)
//   cntlzw r9,r10 ; rlwinm r3,r9,27,31,31 ; blr
// No `addi -1` this time, so the comparison is against zero.
struct E16 { char unk0000[0x0C]; s32 v; };
ASSERT_OFFSET(E16, v, 0x0C);
ASSERT_SIZE(E16, 16);
int IsZeroAt(E16* base, int i) { return base[i].v == 0; }
