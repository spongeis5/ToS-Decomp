// sub_82627BB8 -- find the block that owns a pointer, push it on that block's
// free list, and compact the pool when the block empties. 116 B, 4 callers.
//
//      lwz r11,0(r3) ; cmplwi cr6,r11,0 ; beqlr cr6
//  loop:
//      lwz    r10,12(r11)
//      cmplw  cr6,r4,r10 ; ble- cr6,<false>
//      lwz    r9,4(r11) ; lwz r8,0(r11) ; mullw r9,r9,r8
//      add    r7,r9,r10
//      li     r10,1
//      cmplw  cr6,r4,r7 ; blt- cr6,<test>
//  false:
//      li     r10,0
//  test:
//      clrlwi r10,r10,24 ; cmplwi cr6,r10,0 ; bne- cr6,<hit>
//      lwz    r11,20(r11) ; cmplwi cr6,r11,0 ; bne+ cr6,loop
//      blr
//  hit:
//      lwz    r10,16(r11) ; stw r4,16(r11) ; stw r10,0(r4)
//      lwz    r9,8(r11) ; addic. r10,r9,-1 ; stw r10,8(r11)
//      bnelr
//      b      0x82622BC0
//
// The MATERIALISED-THEN-MASKED bool is the signature of an inlined
// `bool`-returning helper: a bare `if (a && b)` branches out of each term and
// never builds a value. So the range test is its own function, and the pair
// of `li` plus the redundant `clrlwi ...,24` is what inlining it leaves.
// (`clrlwi` and `cmplwi` staying SEPARATE, rather than fusing into `clrlwi.`,
// is the /O2 form of that test -- at /Os it is one instruction.)
//
// `add r7,r9,r10` puts the product in rA, and rA takes the operand whose
// source read comes later: the base at +12 is read by the first comparison,
// the count and size only inside the second term.
//
// The free-list splice needs a NAMED temporary and the stores in the image's
// own order. Written the obvious way -- `*(void**)p = b->freeList;
// b->freeList = p;` -- MSVC sinks the store to +16 past the refcount
// decrement: 24 of 29, with exactly those four words rotated. A load's
// position relative to a store it might alias is source order (sub_82600AD0),
// and here the load has to be lifted out of the second statement for the
// store order to be expressible at all.
//
// `addic.` records into CR0 and `bnelr` reads it, so the decrement and the
// zero test are one expression: `if (--b->used == 0)`.
//
// The tail call takes only r3, which is untouched -- sub_82622BC0 walks the
// pool from `0(r3)` and needs nothing else.
//
// 1 of 29 words is relocated.

#include "types.h"

struct PoolBlock
{
    /* 0x00 */ u32        stride;
    /* 0x04 */ u32        capacity;
    /* 0x08 */ u32        used;
    /* 0x0C */ char*      base;
    /* 0x10 */ void*      freeList;
    /* 0x14 */ PoolBlock* next;
};

ASSERT_OFFSET(PoolBlock, base, 0x0C);
ASSERT_OFFSET(PoolBlock, next, 0x14);

struct Pool
{
    /* 0x00 */ PoolBlock* first;
};

void PoolCompact(Pool* p);

static bool BlockOwns(PoolBlock* b, void* p)
{
    return p > b->base && p < b->base + b->capacity * b->stride;
}

void PoolFree(Pool* pool, void* p)
{
    for (PoolBlock* b = pool->first; b != 0; b = b->next)
    {
        if (BlockOwns(b, p))
        {
            void* next = b->freeList;
            b->freeList = p;
            *(void**)p = next;

            if (--b->used == 0)
                PoolCompact(pool);

            return;
        }
    }
}
