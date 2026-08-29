#include "types.h"

// sub_8219FC90 -- branchless ==, offset 2260. 24 B, 6 callers.
//   lwz r11,8(r3) ; lwz r11,2260(r11) ; addi r10,r11,-1
//   cntlzw r9,r10 ; rlwinm r3,r9,27,31,31 ; blr
// The neighbour of sub_822553C0, which is the same shape at 2264.
struct Deep2260 { char unk0000[0x8D4]; s32 mode; };
struct Top2260  { char unk0000[0x08];  Deep2260* deep; };
ASSERT_OFFSET(Deep2260, mode, 0x8D4);
ASSERT_OFFSET(Top2260,  deep, 0x08);
int IsMode1At2260(Top2260* p) { return p->deep->mode == 1; }
