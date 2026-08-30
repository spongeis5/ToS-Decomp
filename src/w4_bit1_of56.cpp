#include "types.h"

// sub_822038B0 -- a bool accessor for bit 2 of a field reached through the
// object pointer at +0. 16 B, 3 callers.
//
//      lwz     r11,0(r3)        the object
//      lwz     r10,56(r11)      its word at +56
//      rlwinm  r3,r10,30,31,31  rotate right 2, keep the low bit: (x>>2)&1
//      blr
//
// The rotate amount IS the bit index: 30 = rotate right 2, so the bit tested
// is 2 and the bool normalisation rides along. `& 2` was measured and gives
// rotate 31 instead -- one word wrong. The match asserts a bool return; it
// asserts no bitfield layout, so the bit is spelled as a mask.

struct Inner
{
    /* 0x38 */ char unk0000[56];
    /* 0x38 */ u32  word;
};

ASSERT_OFFSET(Inner, word, 56);

struct Outer
{
    Inner* obj;
};

bool Flag1(Outer* o)
{
    return o->obj->word & 4;
}
