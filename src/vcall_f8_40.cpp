#include "types.h"

// sub_8224E080 -- virtual call through a member pointer. 20 B, 16 callers.
//   lwz r3,8(r3) ; lwz r11,0(r3) ; lwz r10,160(r11) ; mtctr r10 ; bctr
// Slot 160/4 = 40. The slot goes to r10, a FRESH register, unlike
// sub_827C5198 where the target reuses r11 -- the allocator's choice is
// context-dependent and neither shape is a fixed habit.
struct T40;
struct VT40 { void* (*slot[41])(T40*); };
struct T40  { VT40* vt; };
struct Own8 { char unk0000[0x08]; T40* member; };
ASSERT_OFFSET(T40,  vt,     0x00);
ASSERT_OFFSET(Own8, member, 0x08);
void* CallSlot40(Own8* o) { T40* t = o->member; return t->vt->slot[40](t); }
