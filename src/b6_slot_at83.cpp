#include "types.h"

// sub_821A6B38 -- read a 4-byte array element 83 places past the base.
// 16 B, 5 callers.
//
//      addi    r11,r4,83
//      rlwinm  r10,r11,2,0,29     (i + 83) * 4
//      lwzx    r3,r10,r3
//      blr
//
// The constant is folded into the INDEX before the scale rather than left as
// a displacement, which is what MSVC does when the base has no displacement
// form available -- the same fold appears in sub_82858720 as `(t + 6) * 24`
// for an array at byte offset 144.
//
// `lwzx` takes the scaled index in rA and the base in rB, which per the
// lwzx-operand-order rule is the free-function `a[i]` form with the array at
// offset 0 -- i.e. a bare pointer parameter, not a member array.

void* SlotAt83(void** a, int i)
{
    return a[i + 83];
}
