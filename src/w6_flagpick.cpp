#include "types.h"

// sub_8276A778 -- pick one of eight globals by flag bits. 180 B, 4 callers.
//
//   f == 0        -> e[0]  (the tail at 8276A820 loads offset 0)
//   (f & 3) == 3  -> e[3]
//   f & 0x10      -> e[5] on bit1, e[6] on bit0, else e[4]
//   else          -> e[7]
//
// The bit4 test's cr6 is set FIRST (780) and consumed by the bne at 790,
// after the (f&3)==3 compare sits between them; every arm re-forms the
// table address with its own lis/addi.

struct PickTable
{
    void* e[8];
};

extern PickTable g_pick_829840A8;

void* PickFlagged(int f)
{
    if (f == 0)
        return g_pick_829840A8.e[0];
    if ((f & 3) == 3)
        return g_pick_829840A8.e[3];
    if (f & 0x10)
    {
        if (f & 2)
            return g_pick_829840A8.e[5];
        if (f & 1)
            return g_pick_829840A8.e[6];
        return g_pick_829840A8.e[4];
    }
    return g_pick_829840A8.e[7];
}

// NEAR-MISS. arm set and polarities right; register/branch schedule differs.
