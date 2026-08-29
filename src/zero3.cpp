#include "types.h"

// sub_8225FDD8 -- zero three consecutive words. 20 B, 7 callers.
//   li r11,0 ; stw r11,0(r3) ; stw r11,4(r3) ; stw r11,8(r3) ; blr
struct Triple { s32 a; s32 b; s32 c; };
ASSERT_OFFSET(Triple, a, 0x00);
ASSERT_OFFSET(Triple, b, 0x04);
ASSERT_OFFSET(Triple, c, 0x08);
void ClearTriple(Triple* t) { t->a = 0; t->b = 0; t->c = 0; }
