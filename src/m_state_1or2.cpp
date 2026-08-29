#include "types.h"

// sub_821A5350 -- "is the state 1 or 2". 36 B, 11 callers.
//
//      lwz     r11,208(r3)
//      cmpwi   cr6,r11,1
//      beq-    cr6,set
//      cmpwi   cr6,r11,2
//      li      r11,0
//      bne-    cr6,out
// set: li      r11,1
// out: clrlwi  r3,r11,24
//      blr
//
// MATCHED -- 9 of 9 words, and the answer was the RETURN TYPE.
//
// THE TRAILING `clrlwi rD,rS,24` IS `bool`, NOT `u8`. This file spent a long
// time reading that mask as "the function returns a byte", which is exactly
// backwards: `u8`, `char` and `int` returns all let MSVC compute the 0/1
// straight into r3 and stop -- 32 bytes, 4 of 9 words, one instruction
// short. Only a `bool` return normalises, and the normalisation is what
// forces the value into r11 first so there is something to normalise FROM.
//
// So the register allocation that looked like the stall was a consequence,
// not a cause. `u8` and `bool` are the same width and the same values here,
// and they are NOT the same code.
//
// Sixteen shapes were compiled at both /O2 and /O2 /Os. Every one that
// returns `bool` is 9 of 9 at both levels -- member, free function, and an
// inlined `bool`-returning helper -- and every one that returns `u8`,
// `char` or `int` is 4 of 9 at both. Branchy spellings (two `if`s, a
// `switch`, an int accumulator) are 2 or 3 of 9 and are not the shape.
// The level carries no information for this function; `/O2` is recorded
// because that is the default.
//
// Companion to the note in MATCHED.md that a materialised-then-masked bool
// is an inlined helper: the mask means `bool` SOMEWHERE, and here it is the
// return type of the function itself.
struct Stateful
{
    char unk0000[0xD0];
    s32  state;

    bool IsBusy() const;
};
ASSERT_OFFSET(Stateful, state, 0xD0);

bool Stateful::IsBusy() const
{
    return state == 1 || state == 2;
}
