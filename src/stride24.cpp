#include "types.h"

// sub_826C0FC8 -- address of an array element, stride 24. 24 B, 11 callers.
//   rlwinm r11,r4,1,0,30   i*2
//   lwz    r10,24(r3)
//   add    r11,r4,r11      i*3
//   rlwinm r11,r11,3,0,28  *8  -> i*24
//   add    r3,r11,r10 ; blr
// The compiler builds *24 as (i + i*2) * 8 rather than a mulli.
//
// WRITTEN AS A MEMBER FUNCTION, and that is the whole reason it matches.
// Six free-function shapes -- index, pointer arithmetic, explicit stride,
// index in a local, base in a local, unsigned index -- all gave 2 of 6 words
// with r10 and r11 exactly SWAPPED against the target. Making it a member
// swapped them back and matched 6/6.
//
// So `this` is not merely the first parameter as far as register allocation
// is concerned. When a function stalls with registers transposed and the
// first argument looks like an object pointer, try the member form before
// anything else.
struct E24 { char unk0000[24]; };
ASSERT_SIZE(E24, 24);

struct Holder24
{
    char unk0000[0x18];
    E24* items;
    E24* At(int i);
};
ASSERT_OFFSET(Holder24, items, 0x18);

E24* Holder24::At(int i) { return &items[i]; }
