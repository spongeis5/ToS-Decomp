#include "types.h"

// sub_8262FE90 -- store the argument into a global object's field at +8.
// 16 B, 3 callers.
//
//      lis     r11,-32092
//      lwz     r11,-23904(r11)   -> [82A3A2A0]; a single lis feeding the load
//                                 directly means a global POINTER variable
//                                 (cf. src/t5_magic_check.cpp: lis+addi is an
//                                 object, lis+displacement is a pointer)
//      stw     r3,8(r11)
//      blr

struct Slot
{
    /* 0x08 */ char unk0000[8];
    /* 0x08 */ void* value;
};

ASSERT_OFFSET(Slot, value, 8);

extern Slot* g_slot_82A3A2A0;

void StoreValue(void* v)
{
    g_slot_82A3A2A0->value = v;
}
