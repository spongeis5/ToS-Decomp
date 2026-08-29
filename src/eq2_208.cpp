#include "types.h"

// sub_821A5378 -- equality test compiled without a branch. 20 B, 8 callers.
//   lwz r11,208(r3) ; addi r11,r11,-2 ; cntlzw r10,r11
//   rlwinm r3,r10,27,31,31 ; blr
// (x-2), count leading zeros, take bit 5 of the count: 1 exactly when x==2.
// This is MSVC's branchless ==, and it appears three times in this batch.
struct S208 { char unk0000[0xD0]; s32 state; };
ASSERT_OFFSET(S208, state, 0xD0);
int IsState2(S208* p) { return p->state == 2; }
