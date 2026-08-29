#include "types.h"

// sub_821A5490 -- conditional field update. 24 B, 6 callers.
//   lwz r11,208(r3) ; cmpwi cr6,r11,1 ; bnelr cr6
//   li r11,2 ; stw r11,208(r3) ; blr
// cmpwi is SIGNED, and the guard is a conditional return.
struct St208 { char unk0000[0xD0]; s32 state; };
ASSERT_OFFSET(St208, state, 0xD0);
void Advance(St208* p) { if (p->state == 1) p->state = 2; }
