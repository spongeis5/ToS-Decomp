#include "types.h"

// sub_825E41D8 -- zero two words. 16 B, 5 callers.
//   li r11,0 ; stw r11,0(r3) ; stw r11,4(r3) ; blr
struct Two { s32 a; s32 b; };
ASSERT_OFFSET(Two, a, 0x00);
ASSERT_OFFSET(Two, b, 0x04);
void ClearTwo(Two* t) { t->a = 0; t->b = 0; }
