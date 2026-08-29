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
// Two compares against consecutive constants, folded to one boolean. The
// trailing `clrlwi rD,rS,24` is the zero-extension of a byte-sized return,
// so the function returns u8 and not int -- an int return would have left
// r11 alone.
struct Stateful
{
    char unk0000[0xD0];
    s32  state;

    u8 IsBusy() const;
};
ASSERT_OFFSET(Stateful, state, 0xD0);

// NOT MATCHED -- 4 of 8 words. The instructions are right and the target
// keeps the boolean in r11, zero-extending it into r3 only at the end; ours
// computes straight into r3 and comes out 4 bytes shorter. Tried: the plain
// expression, a named u8 local (worse, 1 of 8), and the member form (also
// 4 of 8). It is a register-allocation choice, which is the same wall the
// six older stalls sit behind.
u8 Stateful::IsBusy() const
{
    return (u8)(state == 1 || state == 2);
}
