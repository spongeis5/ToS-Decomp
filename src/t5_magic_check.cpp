#include "types.h"

// sub_8225D500 -- is the global's object present and stamped with 'MNUS'?
// 56 B, 6 callers.
//
//      lis     r11,-32101 ; addi r10,r11,-19728    -> 829AB2F0, a global OBJECT
//      lwz     r11,1084(r10)                       its pointer field
//      cmplwi  cr6,r11,0 ; bne- -> 8225D51C
//      li      r3,0 ; blr
//      lis     r10,19790 ; lwz r9,0(r11) ; ori r8,r10,21843   0x4D4E5553
//      subf    r7,r9,r8 ; cntlzw r6,r7 ; rlwinm r3,r6,27,31,31
//      blr
//
// `lis`+`addi` and then a displacement in the load is the address of a global
// OBJECT, not a global pointer variable -- compare src/b_fwd_global5.cpp,
// which is a single `lis` feeding the load directly.
//
// `cntlzw` + `rlwinm rX,rY,27,31,31` with no `addi -1` in front is branchless
// `x == 0`, so the comparison is materialised rather than branched.
// `subf rD,rA,rB` computes rB - rA and MSVC emits `a == b` that way, so rA is
// the left operand: the source reads `p->magic == 0x4D4E5553`, not the
// reverse.
//
// The null test is `bne-` AWAY to the interesting path with the zero return as
// the fall-through, which is the early-return spelling `if (p == 0) return 0;`
// -- the opposite polarity from writing the interesting path first.

struct Stream
{
    /* 0x00 */ u32 magic;
};
ASSERT_OFFSET(Stream, magic, 0x00);

struct StreamManager
{
    /* 0x000 */ char    unk0000[0x43C];
    /* 0x43C */ Stream* stream;
};
ASSERT_OFFSET(StreamManager, stream, 0x43C);

extern StreamManager g_streamManager;

int IsStreamOpen()
{
    Stream* s = g_streamManager.stream;
    if (s == 0)
        return 0;
    return s->magic == 0x4D4E5553;
}
