#include "types.h"

// sub_82724A98 -- branchless signed >= , stored as a byte. 28 B, 16 callers.
//
//      lwz     r11,8(r4)        a = p->value
//      rlwinm  r10,r5,1,31,31   sign bit of b, as 0 or 1
//      srawi   r9,r11,31        0 or -1: minus the sign bit of a
//      subfc   r8,r5,r11        a - b, and CA = 1 when a >=u b
//      adde    r11,r10,r9       sign(b) - sign(a) + carry
//      stb     r11,0(r3)
//      blr
//
// This is the standard branchless signed comparison and it is worth writing
// out, because reading it as arithmetic wastes an afternoon: `subfc` is
// there ONLY for the carry -- its difference in r8 is never used -- and the
// two sign bits correct that unsigned carry into a signed answer.
//
//   both non-negative   0 - 0 + (a >=u b)  = a >= b
//   a < 0 <= b         0 - 1 + 1           = 0
//   b < 0 <= a         1 - 0 + 0           = 1
//   both negative      1 - 1 + (a >=u b)   = a >= b
//
// So the whole sequence is one `>=` on two signed ints, with the result
// stored as a byte rather than branched on.
struct Compared
{
    char unk0000[8];
    s32  value;
};
ASSERT_OFFSET(Compared, value, 8);

void StoreAtLeast(u8* out, const Compared* p, int limit)
{
    *out = (u8)(p->value >= limit);
}
