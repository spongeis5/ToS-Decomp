#include "types.h"

// sub_82649240 -- zero three fields, NOT in address order. 20 B, 6 callers.
//   li r11,0 ; stw r11,64(r3) ; stw r11,68(r3) ; stw r11,0(r3) ; blr
// Written in the target's own store order: 64, 68, then 0.
struct Res { s32 handle; char unk0004[0x3C]; s32 a; s32 b; };
ASSERT_OFFSET(Res, handle, 0x00);
ASSERT_OFFSET(Res, a,      0x40);
ASSERT_OFFSET(Res, b,      0x44);
void ResetRes(Res* r) { r->a = 0; r->b = 0; r->handle = 0; }
