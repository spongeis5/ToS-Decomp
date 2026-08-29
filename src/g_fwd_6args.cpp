#include "types.h"

// sub_822E2048 -- shift two arguments into the middle of a six-argument call
// and tail call. 28 B, 15 callers.
//
//      mr      r7,r4
//      mr      r5,r3
//      li      r8,1
//      li      r6,0
//      li      r4,0
//      li      r3,0
//      b       0x822E1FE8
//
// The trailing `b` is the tail call. `mr` rather than `rlwinm ...,0,0,31`
// says the two forwarded arguments are pointer or signed typed.

int RunRequest(int a, int b, void* c, int d, void* e, int f);

int RunRequestDefault(void* c, void* e)
{
    return RunRequest(0, 0, c, 0, e, 1);
}
