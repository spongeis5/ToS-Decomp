#include "types.h"

// sub_826A3328 -- initialise a small record. 36 B, 18 callers.
//
//      li      r11,0
//      stw     r4,20(r3)        +0x14 FIRST
//      li      r10,512
//      stw     r11,0(r3)
//      stw     r11,4(r3)
//      stw     r11,8(r3)
//      stw     r10,12(r3)       0x200
//      stb     r11,16(r3)       a byte, not a word
//      blr
//
// Store order is source order: the argument lands at +0x14 before any of the
// constants, which is not field order.
struct Sink
{
    s32   f00;
    s32   f04;
    s32   f08;
    s32   capacity;
    u8    flag;
    char  unk0011[3];
    void* owner;
};
ASSERT_OFFSET(Sink, capacity, 0x0C);
ASSERT_OFFSET(Sink, flag, 0x10);
ASSERT_OFFSET(Sink, owner, 0x14);

void InitSink(Sink* s, void* owner)
{
    s->owner    = owner;
    s->f00      = 0;
    s->f04      = 0;
    s->f08      = 0;
    s->capacity = 512;
    s->flag     = 0;
}
