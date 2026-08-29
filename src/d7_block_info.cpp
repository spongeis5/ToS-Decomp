// sub_8252F5D8 -- report a buffer's block size and block count through two
// optional out-parameters, always returning 0. 56 B, 5 callers.
//
//      mr      r11,r3             save the object; r3 becomes the result
//      cmplwi  cr6,r4,0
//      beq-    cr6,0x8252F5EC
//      lwz     r10,1112(r3)       b->blockSize   (+0x458)
//      stw     r10,0(r4)
//  8252F5EC:
//      cmplwi  cr6,r5,0
//      li      r3,0               the return value, materialised early
//      beqlr   cr6
//      lwz     r9,1112(r11)       RELOAD of b->blockSize
//      lwz     r10,1116(r11)      b->total       (+0x45C)
//      twllei  r9,0               the divide-by-zero trap
//      divwu   r8,r10,r9          total / blockSize, UNSIGNED
//      stw     r8,0(r5)
//      blr
//
// `mr r11,r3` exists only because r3 is the RETURN register -- the constant 0
// is materialised into it before the second guard and the object has to move
// out of the way. src/u3_init_zero.cpp is the same shape and the same reason;
// a void function would keep everything in r3.
//
// `divwu` and `twllei` (trap if logically <= 0) are the unsigned pair, so
// both fields are u32. A signed divide would be `divw` with `twllei`'s signed
// twin around it.
//
// The RELOAD of +0x458 is forced: `*blockSize = b->blockSize` stores through a
// caller-supplied pointer that may alias the object, so the second read
// cannot be common-subexpressioned with the first.
//
// The single `li r3,0` shared by both exits, with the second guard spelled as
// `beqlr` rather than a branch, is one `return 0` at the END and no early
// return in the source.
//
// Nothing is relocated; all 14 words are compared.

#include "types.h"

struct Buffer
{
    /* 0x000 */ char unk0000[0x458];
    /* 0x458 */ u32  blockSize;
    /* 0x45C */ u32  total;
};

ASSERT_OFFSET(Buffer, blockSize, 0x458);
ASSERT_OFFSET(Buffer, total,     0x45C);

int GetBlockInfo(Buffer* b, u32* blockSize, u32* blockCount)
{
    if (blockSize)
        *blockSize = b->blockSize;
    if (blockCount)
        *blockCount = b->total / b->blockSize;
    return 0;
}
