// sub_822481D8 -- forward an item to the current target, unless a global
// flag is set. 44 B, 3 callers.
//
//      lis    r11,-32108
//      mr     r4,r3
//      lbz    r10,-24616(r11)      = 82939FD8
//      cmplwi cr6,r10,0
//      bnelr  cr6                  the flag is SET -> do nothing
//      cmplwi cr6,r3,0
//      beqlr  cr6
//      lis    r11,-32102
//      lwz    r3,29924(r11)        = [829A74E4]
//      b      0x82247688
//
// Two globals, both READ rather than addressed: a byte flag at 82939FD8 and
// a pointer at 829A74E4.  `lis` with the low half folded into the load's
// displacement is the read form; taking an address would need an `addi`.
//
// The argument is moved to r4 at the top because r3 has to carry the global
// pointer into the call -- the callee takes (target, item) in that order.
// The `mr` is scheduled into the `lis`/`lbz` gap, not written early.
//
// Both guards return, and the call is the fall-through, so the guards are
// written first and the interesting path last.  `bnelr` on the flag and
// `beqlr` on the pointer are the two polarities of the same shape.

#include "types.h"

struct Item;
struct Target;

void PostToTarget(Target* t, Item* item);

extern u8      g_posting_disabled;      /* 82939FD8 */
extern Target* g_current_target;        /* 829A74E4 */

void PostItem(Item* item)
{
    if (g_posting_disabled)
        return;
    if (item == 0)
        return;

    PostToTarget(g_current_target, item);
}
