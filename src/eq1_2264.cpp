#include "types.h"

// sub_822553C0 -- the same branchless == through two loads. 24 B, 6 callers.
//   lwz r11,8(r3) ; lwz r11,2264(r11) ; addi r10,r11,-1
//   cntlzw r9,r10 ; rlwinm r3,r9,27,31,31 ; blr
struct Deep { char unk0000[0x8D8]; s32 mode; };
struct Top  { char unk0000[0x08];  Deep* deep; };
ASSERT_OFFSET(Deep, mode, 0x8D8);
ASSERT_OFFSET(Top,  deep, 0x08);
int IsMode1(Top* p) { return p->deep->mode == 1; }
