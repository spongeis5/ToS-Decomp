// sub_822E0D80 -- swap the first two arguments and tail-call. 16 B,
// 4 callers.
//
//      mr r11,r4 ; mr r4,r3 ; mr r3,r11 ; b 0x822DAE30
//
// A three-move permutation, so the second argument goes first. The callee
// reads r3..r7 (sub_822DAE30 uses r6 and r7 in its first six instructions)
// and the only caller inspected, sub_82159198, sets all five, so the thunk
// carries five parameters and passes the last three through untouched --
// they generate no code, but declaring only two would be a claim the callers
// contradict.
//
// The tail call is `b`, not `bl`, so the return type has to be one the
// callee's own return satisfies; nothing here reads a result.

#include "types.h"

void Callee(void* a, void* b, void* c, s32 d, s32 e);

void SwapForward(void* a, void* b, void* c, s32 d, s32 e)
{
    Callee(b, a, c, d, e);
}
