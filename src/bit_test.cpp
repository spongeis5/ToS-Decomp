#include "types.h"

// sub_825BD930 -- test one bit of a byte. 16 B, 6 callers.
//   lwz r11,0(r3) ; lbz r10,5(r11) ; rlwinm r3,r10,0,30,30 ; blr
// rlwinm with rotate 0 and mask 30..30 keeps bit 1 in place: x & 2.
struct Flags  { char unk0000[0x05]; u8 flags; };
struct Owner5 { Flags* f; };
ASSERT_OFFSET(Flags,  flags, 0x05);
ASSERT_OFFSET(Owner5, f,     0x00);
int TestBit1(Owner5* o) { return o->f->flags & 2; }
