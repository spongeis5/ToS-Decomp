#include "types.h"

// sub_82600A88 -- unlink a node and push it on a free list. 56 B, 13 callers.
//
//      mr      r10,r3
//      lwz     r3,4(r4)      nx = n->next -- loaded first, and returned
//      lwz     r9,0(r4)
//      stw     r9,0(r3)      nx->prev = n->prev
//      lwz     r8,4(r4)      RELOADED
//      lwz     r7,0(r4)      RELOADED
//      stw     r8,4(r7)      n->prev->next = n->next
//      lwz     r6,12(r10)
//      stw     r6,4(r4)      n->next = l->free
//      lwz     r11,0(r10)
//      addi    r5,r11,-1
//      stw     r4,12(r10)    l->free = n
//      stw     r5,0(r10)     --l->count
//      blr
//
// The RELOADS are the whole shape. `n->next` and `n->prev` are read twice
// because the store in between (`nx->prev = ...`) might alias `n` -- MSVC
// cannot prove otherwise through two unrelated pointers. So the first
// statement uses the named local and the second spells the chain out; a
// source that named both would load once and not match, and a source that
// named neither would reload three times.
struct LNode
{
    LNode* prev;
    LNode* next;
};

struct LList
{
    s32    count;
    char   unk0004[8];
    LNode* free;
};
ASSERT_OFFSET(LList, free, 0x0C);

LNode* UnlinkToFree(LList* l, LNode* n)
{
    LNode* nx = n->next;
    nx->prev = n->prev;
    n->prev->next = n->next;
    n->next = l->free;
    l->free = n;
    --l->count;
    return nx;
}
