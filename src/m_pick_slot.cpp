#include "types.h"

// sub_82202D08 -- pick one of two adjacent sub-objects, or fall back. 48 B.
//
//      lbz     r11,526(r3)      +0x20E
//      cmplwi  cr6,r11,0
//      beq-    cr6,other
//      lbz     r10,524(r3)      +0x20C
//      addi    r11,r3,484       &this->a   (+0x1E4)
//      cmplwi  cr6,r10,0
//      beq-    cr6,out
//      addi    r11,r11,20       ++p
// out: mr      r3,r11
//      blr
// other:lwz    r3,168(r3)
//      blr
//
// `addi r11,r11,20` adds to the pointer ALREADY in r11 rather than computing
// `this + 504` outright, so the source advanced a pointer it had -- `++p` on
// a 20-byte type -- and did not name the second sub-object. The stride is
// what fixes the size at 20; the two flags sitting at 0x20C and 0x20E, right
// after 0x1F8 + 20 = 0x20C, confirm the two slots are adjacent.
struct Slot
{
    char unk0000[20];
};
ASSERT_SIZE(Slot, 20);

struct Chooser
{
    char  unk0000[0xA8];
    Slot* fallback;
    char  unk00AC[0x1E4 - 0xAC];
    Slot  slots[2];
    u8    useSecond;
    u8    unk020D;
    u8    enabled;
};
ASSERT_OFFSET(Chooser, fallback, 0xA8);
ASSERT_OFFSET(Chooser, slots, 0x1E4);
ASSERT_OFFSET(Chooser, useSecond, 0x20C);
ASSERT_OFFSET(Chooser, enabled, 0x20E);

Slot* PickSlot(Chooser* c)
{
    if (!c->enabled)
        return c->fallback;

    Slot* p = &c->slots[0];
    if (c->useSecond)
        ++p;
    return p;
}
