#include "types.h"

// sub_827FE808 -- clear bits in a byte. 16 B, 5 callers.
//   lbz r11,106(r3) ; andi. r11,r11,243 ; stb r11,106(r3) ; blr
// 243 is 0xF3, so bits 2 and 3 are cleared.
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags &= 243; }
