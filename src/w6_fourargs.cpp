#include "types.h"

// sub_825FC7E0 -- prepend three constants and tail-call. 20 B, 3 callers.
//
//      lis     r6,8192
//      li      r5,0
//      ori     r6,r6,4            = 0x20000004
//      li      r4,-1
//      b       822F8730
//
// r3 passes through untouched, so the first argument is the caller's own.

int Tail_822F8730(int, int, int, int);

int Call4(int x)
{
    return Tail_822F8730(x, -1, 0, 0x20000004);
}
