// sub_8262FB50 -- release a block that carries a 16-byte header, stamping the
// header with a 0xDEADBEEF free sentinel on the way out. 48 bytes, 37 callers.
//
//      mr      r11,r4              keep the payload pointer
//      cmplwi  cr6,r4,0
//      beqlr   cr6                 free(NULL) is a no-op
//      lis     r9,-8531            0xDEAD....
//      lwz     r10,-12(r4)         header.size
//      lwz     r6,-8(r11)          header.tag
//      addi    r4,r4,-16           the header itself
//      ori     r8,r9,48879         0xDEADBEEF
//      addi    r5,r10,16           size + 16
//      stw     r8,-16(r11)         header.sentinel = 0xDEADBEEF
//      b       0x8262F658          tail call
//
// The two loads are emitted BEFORE the sentinel store, which is the source
// order: a store through the same pointer would otherwise have to be assumed
// to clobber them.
//
// The tail-called routine takes (allocator, block, byte count, tag) and is
// shared with sub_82637590, which reaches the same entry with a size it
// computes itself. r3 is passed straight through untouched.

#include "types.h"

struct Allocator;

struct BlockHeader
{
    /* 0x00 */ u32 sentinel;
    /* 0x04 */ u32 size;
    /* 0x08 */ u32 tag;
    /* 0x0C */ u32 unk000C;
};

ASSERT_OFFSET(BlockHeader, sentinel, 0x00);
ASSERT_OFFSET(BlockHeader, size,     0x04);
ASSERT_OFFSET(BlockHeader, tag,      0x08);
ASSERT_SIZE(BlockHeader, 16);

void ReleaseBlock(Allocator* a, void* block, u32 bytes, u32 tag);

void FreeBlock(Allocator* a, void* p)
{
    BlockHeader* h;
    u32 size;
    u32 tag;

    if (p == 0)
        return;

    h = (BlockHeader*)p - 1;
    size = h->size;
    tag = h->tag;
    h->sentinel = 0xDEADBEEF;
    ReleaseBlock(a, h, size + 16, tag);
}
