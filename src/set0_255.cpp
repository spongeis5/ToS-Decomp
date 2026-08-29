#include "types.h"

// sub_82265D30 -- store 0 and 255. 20 B, 7 callers.
//   li r11,0 ; li r10,255 ; stw r11,12(r3) ; stw r10,16(r3) ; blr
// Both constants are materialised before either store.
struct Pair { char unk0000[0x0C]; s32 count; s32 limit; };
ASSERT_OFFSET(Pair, count, 0x0C);
ASSERT_OFFSET(Pair, limit, 0x10);
void InitPair(Pair* p) { p->count = 0; p->limit = 255; }
