#include "types.h"

// sub_82218F90 -- scale one argument by four, rotate the rest down a slot,
// prepend a dereference of the first and tail call. 28 B, 13 callers.
//
//      lwz     r10,0(r3)        *o -- read before r3 is clobbered
//      mr      r11,r5           n saved before r5 is overwritten
//      mr      r5,r4
//      li      r6,0
//      mr      r4,r10
//      rlwinm  r3,r11,2,0,29    n * 4
//      b       0x82602EA0
//
// Written with the dereference inline -- `Submit(n * 4, *o, arg, 0)` -- this
// scores 1 of 7: MSVC frees r4 first (`mr r11,r4`) and then loads STRAIGHT
// into r4, so every shuffle lands in a different register. Naming the
// dereference as a local pulls the load to the front, before any argument
// register is disturbed, and it then has to go via a scratch and be copied --
// which is the target's `lwz r10,0(r3)` ... `mr r4,r10`. 7 of 7.
//
// Same lever as src/a_vcall4_or_neg1.cpp and pointing the same way for once:
// there un-naming produced the copy, here naming does. What the local
// actually decides is WHEN the value is computed; the copy is a consequence
// of computing it while the register it belongs in is still occupied.

int Submit(u32 bytes, void* head, void* arg, int flags);

int SubmitCount(void** o, void* arg, u32 n)
{
    void* head = *o;
    return Submit(n * 4, head, arg, 0);
}
