// sub_82666360 -- recorded as 28 bytes / 25 callers, and the inventory has
// MERGED TWO FUNCTIONS. The listing is unambiguous about it:
//
//  82666360  mr r4,r3 ; lwz r3,24(r3) ; b 0x826661E0     <- 12 bytes
//  8266636C  .long 0x0                                   <- alignment padding
//  82666370  mr r4,r3 ; lwz r3,24(r3) ; b 0x82666290     <- a second body
//
// A zero word in the middle of a "function" that is immediately followed by
// a fresh prologue-shaped sequence is inter-function alignment padding, and
// 82666370 is 16-byte aligned exactly as a COMDAT start would be. 12 + 4 +
// 12 = 28, which is where the recorded size comes from.
//
// The member at +0x18 becomes the callee's first argument and the object
// itself becomes the second, so the callee is something like
// `Sink::Take(target, owner)`.
//
// MEASURED against both halves of the row: 3 words each, 2 identical and 1
// relocated (the tail branch) for the body at 82666360 AND for the body at
// 82666370. One source reproduces both, which is the proof that the row is
// two copies of this shape with different callees.
//
// It therefore cannot report MATCH: match.py requires len(code) == recorded
// size, and 12 != 28.

#include "types.h"

struct Owner
{
    /* 0x00 */ char  unk0000[0x18];
    /* 0x18 */ void* member;
};

ASSERT_OFFSET(Owner, member, 0x18);

void Take(void* target, Owner* owner);

void HandOff(Owner* o)
{
    Take(o->member, o);
}
