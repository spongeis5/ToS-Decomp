#include "types.h"

// sub_8214CC48 -- set a bit in a word. 16 B, 6 callers.
//   lwz r11,148(r3) ; ori r10,r11,1 ; stw r10,148(r3) ; blr
struct F148 { char unk0000[0x94]; u32 flags; };
ASSERT_OFFSET(F148, flags, 0x94);
void SetBit0(F148* p) { p->flags |= 1; }
