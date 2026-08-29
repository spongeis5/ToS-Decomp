#include "types.h"

// sub_822021F8 -- field of an array element, stride 116. 16 B, 14 callers.
//   mulli r11,r4,116 ; add r11,r11,r3 ; lwz r3,16(r11) ; blr
// mulli rather than shifts because 116 is not a power of two.
struct E116 { char unk0000[0x10]; void* v; char unk0014[116 - 0x14]; };
ASSERT_OFFSET(E116, v, 0x10);
ASSERT_SIZE(E116, 116);
void* At116(E116* base, int i) { return base[i].v; }
