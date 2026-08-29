#include "types.h"

// sub_82727028 -- store a pointer and a computed offset. 20 B, 6 callers.
//   lhz r11,18(r5) ; stw r5,4(r3) ; add r11,r11,r4 ; stw r11,0(r3) ; blr
//
// The order is: LOAD s->base, store s, then add and store the sum. Writing
// `o->src = s; o->value = s->base + add;` put the store first and the load
// second -- the same instructions, one position apart. Reading the base into
// a local before the first store sequences it the way the target has it.
struct Src18 { char unk0000[0x12]; u16 base; };
struct Out   { s32 value; Src18* src; };
ASSERT_OFFSET(Src18, base,  0x12);
ASSERT_OFFSET(Out,   value, 0x00);
ASSERT_OFFSET(Out,   src,   0x04);

void MakeOut(Out* o, int add, Src18* s)
{
    int base = s->base;
    o->src = s;
    o->value = base + add;
}
