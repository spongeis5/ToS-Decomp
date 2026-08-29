#include "types.h"

// sub_822CEE08 -- index into a chain of 16-element chunks. 64 B, 9 callers.
//
//      lwz     r11,136(r3)     n = this->head
//      cmplwi  cr6,r11,0
//      beq-    cr6,null
//  L:  cmplwi  cr6,r4,16
//      blt-    cr6,hit         <- the break
//      lwz     r11,960(r11)    n = n->next   (960 == 16 * 60)
//      addi    r4,r4,-16
//      cmplwi  cr6,r11,0
//      bne+    cr6,L
// null:li      r3,0
//      blr
// hit: cmplwi  cr6,r11,0
//      beq+    cr6,null
//      mulli   r10,r4,60
//      add     r3,r10,r11
//      blr
//
// The null test is DUPLICATED: the two loop-exit paths (the entry guard and
// the back edge) both leave with n == 0 and jump straight to the zero return,
// while the `break` path gets its own `cmplwi`/`beq`. That is a rotated
// `while (n != 0)` whose body starts with the break -- the compiler value-
// propagates the null on the exits it controls and re-tests on the one it
// does not.
//
// `next` at 960 with a `mulli ...,60` on the residue fixes the layout: sixteen
// 60-byte elements at offset 0, then the link.
//
// BRANCH POLARITY decides whether the zero return is emitted once or twice.
// `if (n == 0) return 0; return &n->items[i];` puts the zero on the
// fall-through, so the compiler lays a SECOND copy of `li r3,0 ; blr` after
// the break's null test -- 72 bytes and five wrong words. Writing the
// interesting path first turns that test into a BACKWARD `beq+` into the one
// zero return that is already there: 64 bytes, 16 of 16.
struct Chunk;

struct Element
{
    char unk0000[60];
};
ASSERT_SIZE(Element, 60);

struct Chunk
{
    /* 0x000 */ Element items[16];
    /* 0x3C0 */ Chunk*  next;
};
ASSERT_OFFSET(Chunk, next, 960);

struct ChunkList
{
    /* 0x00 */ char   unk0000[0x88];
    /* 0x88 */ Chunk* head;

    Element* At(u32 i);
};
ASSERT_OFFSET(ChunkList, head, 0x88);

Element* ChunkList::At(u32 i)
{
    Chunk* n = head;
    while (n != 0)
    {
        if (i < 16)
            break;
        n = n->next;
        i -= 16;
    }
    if (n != 0)
        return &n->items[i];
    return 0;
}
