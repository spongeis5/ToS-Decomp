#include "types.h"

// sub_827A7C98 -- store an argument and a global address. 20 B, 5 callers.
//   lis r11,-32134 ; stw r4,8(r3) ; addi r10,r11,31880 ; stw r10,4(r3) ; blr
// Store order is 8 then 4; the address computation is scheduled between.
struct Thing;
extern Thing g_thing_827A7C98;
struct Rec { char unk0000[0x04]; Thing* t; void* p; };
ASSERT_OFFSET(Rec, t, 0x04);
ASSERT_OFFSET(Rec, p, 0x08);
void InitRec(Rec* r, void* p) { r->p = p; r->t = &g_thing_827A7C98; }
