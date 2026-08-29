#include "types.h"

// sub_822CEE48 -- index into a chain of 4-element chunks. 64 B, 6 callers.
//
// The IMMEDIATE NEIGHBOUR of sub_822CEE08 (src/k_chunk_at.cpp): same 64-byte
// body, same instruction order, same branch polarity, differing only in the
// head offset, the chunk arity and the element stride.
//
//      lwz     r11,144(r3)     n = this->head      (+0x90)
//      cmplwi  cr6,r11,0
//      beq-    cr6,null
//  L:  cmplwi  cr6,r4,4
//      blt-    cr6,hit         <- the break
//      lwz     r11,128(r11)    n = n->next   (128 == 4 * 32)
//      addi    r4,r4,-4
//      cmplwi  cr6,r11,0
//      bne+    cr6,L
// null:li      r3,0
//      blr
// hit: cmplwi  cr6,r11,0
//      beq+    cr6,null
//      rlwinm  r10,r4,5,0,26   i * 32
//      add     r3,r10,r11
//      blr
//
// `next` at 128 with a `rlwinm ...,5,0,26` on the residue fixes the layout:
// four 32-byte elements at offset 0, then the link.
//
// The null test is DUPLICATED for the same reason it is in the neighbour: the
// two loop exits that leave with n == 0 jump straight to the zero return, and
// the `break` path re-tests. Writing the interesting path first is what turns
// that test into a BACKWARD `beq+` into the single zero return rather than
// laying a second copy of it down.
struct Chunk32;

struct Element32
{
    char unk0000[32];
};
ASSERT_SIZE(Element32, 32);

struct Chunk32
{
    /* 0x00 */ Element32 items[4];
    /* 0x80 */ Chunk32*  next;
};
ASSERT_OFFSET(Chunk32, next, 128);

struct Chunk32List
{
    /* 0x00 */ char     unk0000[0x90];
    /* 0x90 */ Chunk32* head;

    Element32* At(u32 i);
};
ASSERT_OFFSET(Chunk32List, head, 0x90);

Element32* Chunk32List::At(u32 i)
{
    Chunk32* n = head;
    while (n != 0)
    {
        if (i < 4)
            break;
        n = n->next;
        i -= 4;
    }
    if (n != 0)
        return &n->items[i];
    return 0;
}
