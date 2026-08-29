// sub_821A2258 -- "is the field not -1". 24 B, 3 callers.
//
//      lwz   r11,8(r3)
//      lwz   r11,1392(r11)
//      addi  r11,r11,1
//      addic r10,r11,-1
//      subfe r3,r10,r11
//
// `addic rD,rS,-1 ; subfe rT,rD,rS` is the branchless `!= 0` from the idiom
// table, and the `addi +1` in front of it makes the tested value `x + 1`.
// So the predicate is `x != -1`, MSVC's ordinary spelling of it: it forms
// x - (-1) and asks whether that is non-zero.
//
// The 0/1 lands in r3 with no trailing clrlwi, which is the `int` shape --
// src/m_ready_not255.cpp ends in the same two instructions and is also int.
//
// The chained load reuses r11 for both steps, which is what spelling the
// chain out gives rather than naming the inner object in a local.

#include "types.h"

struct Slot1392
{
    /* 0x0000 */ char unk0000[0x570];
    /* 0x0570 */ s32  id;
};
ASSERT_OFFSET(Slot1392, id, 0x570);

struct Slot1392Owner
{
    /* 0x00 */ char       unk0000[0x08];
    /* 0x08 */ Slot1392*  slot;
};
ASSERT_OFFSET(Slot1392Owner, slot, 0x08);

int HasId(Slot1392Owner* o)
{
    return o->slot->id != -1;
}
