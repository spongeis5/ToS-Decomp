// sub_82727138 -- address of a 4-byte array element through a pointer held at
// offset 0. 16 bytes, 3 callers.
//
//      lwz     r11,0(r3)
//      rlwinm  r10,r4,2,0,29       i * 4
//      add     r3,r10,r11
//      blr
//
// The base comes from a LOAD, not from a relocated lis/addi pair, so the
// array is reached through a member pointer rather than being a global.
// `rlwinm ...,2,0,29` is the 4-byte scale from the idiom table.
//
// `add r3,r10,r11` puts the scaled index in rA, which is the same order
// src/stride24.cpp has for the same shape.
//
// Nothing is relocated: 4 of 4 words are compared.

#include "types.h"

struct WordArray
{
    /* 0x00 */ s32* items;
};

s32* WordAt(WordArray* a, int i)
{
    return &a->items[i];
}
