// sub_82271898 -- do nothing unless a counter is non-zero. 20 bytes,
// 3 callers.
//
//      lwz     r11,68(r3)
//      cmpwi   cr6,r11,0
//      beqlr   cr6
//      b       0x82250F30
//      blr                          <- unreachable, and counted in the size
//
// `cmpwi` and not `cmplwi`: a SIGNED zero test, and the value is never
// dereferenced, so the field is an int and not a pointer. Every pointer null
// test in this image is `cmplwi`.
//
// `beqlr` is the conditional-RETURN idiom, so the guard is written as the
// early exit and the call is the fall-through -- the positive path first.
//
// The trailing `blr` is the dead one MSVC appends after a tail call; here the
// recorded size includes it, so all five words are in the window.
//
// The tail branch is relocated, so 4 of 5 words are compared.

#include "types.h"

struct Pending
{
    /* 0x00 */ u8  unk0000[68];
    /* 0x44 */ s32 count;
};

ASSERT_OFFSET(Pending, count, 68);

void FlushPending(Pending* p);

void FlushIfAny(Pending* p)
{
    if (p->count != 0)
        FlushPending(p);
}
