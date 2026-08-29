// sub_821A93E0 -- "is the sub-object's kind 2". 24 B, 4 callers.
//
//   lwz    r11,144(r3)
//   lwz    r11,36(r11)
//   addi   r10,r11,-2
//   cntlzw r9,r10
//   rlwinm r3,r9,27,31,31
//
// `addi -N ; cntlzw ; rlwinm rX,rY,27,31,31` is the branchless `x == N`.
//
// This is sub_821A93C8 (src/eq1_144_36.cpp) again, 24 bytes earlier, on the
// same two field offsets 0x90 and 0x24 and differing only in the constant --
// so the two are the same predicate over the same layout for two adjacent
// kind values, and the layout here is copied from the one that matched.
//
// The chained load REUSES r11 for both steps, which is what spelling the
// chain out (rather than naming the sub-object in a local) gives.

#include "types.h"

struct Sub36
{
    /* 0x00 */ char unk0000[0x24];
    /* 0x24 */ s32  kind;
};
ASSERT_OFFSET(Sub36, kind, 0x24);

struct Own144
{
    /* 0x00 */ char   unk0000[0x90];
    /* 0x90 */ Sub36* sub;
};
ASSERT_OFFSET(Own144, sub, 0x90);

int IsKind2(Own144* p)
{
    return p->sub->kind == 2;
}
