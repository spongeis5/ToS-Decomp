// sub_82218FB0 -- scale one argument by twelve, rotate the rest down a slot,
// prepend a dereference of the first and tail call sub_82602EA0. 36 B,
// 4 callers.
//
//      rlwinm  r10,r5,1,0,30    n * 2
//      lwz     r9,0(r3)         *o -- read before r3 is clobbered
//      mr      r11,r5           n saved, and then NEVER READ
//      add     r8,r5,r10        n + n*2 = n*3
//      mr      r5,r4
//      li      r6,0
//      mr      r4,r9
//      rlwinm  r3,r8,2,0,29     (n*3) * 4 = n * 12
//      b       0x82602EA0
//
// This is src/g_fwd_shift4.cpp (sub_82218F90, 32 bytes earlier, same tail
// call) with the scale changed from 4 to 12, and it wants the same shape:
// the dereference NAMED in a local, so the load is issued before any argument
// register is disturbed and then copied into r4.
//
// `n * 12` decomposed into shift/add/shift rather than `mulli r3,r5,12` is
// the /O2 signature -- see MATCHED.md, where all ten spellings of a stride
// emit `mulli` at /O2 /Os and none does at /O2. So the level is readable off
// this function without compiling it.
//
// The `mr r11,r5` is DEAD: r5 is still live when `add r8,r5,r10` reads it,
// and r11 is never read again. It is the same copy g_fwd_shift4 needs for
// real, emitted here by the same source shape in a schedule that no longer
// needs it.
//
// The `b` is relocated; the other 8 words are compared.

#include "types.h"

int Submit(u32 bytes, void* head, void* arg, int flags);

int SubmitCount12(void** o, void* arg, u32 n)
{
    void* head = *o;
    return Submit(n * 12, head, arg, 0);
}
