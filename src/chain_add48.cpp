#include "types.h"

// sub_82639C28 -- two loads then a constant offset. 16 B, 11 callers.
//   lwz r11,48(r3) ; lwz r11,24(r11) ; addi r3,r11,48 ; blr
struct Inner { /* 0x18 */ char unk0000[0x18]; char* buf; };
struct Outer { /* 0x30 */ char unk0000[0x30]; Inner* inner; };
ASSERT_OFFSET(Inner, buf,   0x18);
ASSERT_OFFSET(Outer, inner, 0x30);
char* BufPlus48(Outer* o) { return o->inner->buf + 48; }
