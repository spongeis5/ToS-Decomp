// sub_826009D8 -- unlink the first node of an embedded-head doubly linked
// list, push it on a singly linked free list and decrement the count.
// 48 B, 5 callers.
//
//      lwz     r10,8(r3)          n  = p->head.next     (+0x08)
//      addi    r9,r3,4            &p->head              (+0x04)
//      lwz     r8,4(r10)          nx = n->next
//      stw     r8,8(r3)           p->head.next = nx
//      stw     r9,0(r8)           nx->prev = &p->head
//      lwz     r7,12(r3)          p->freeList           (+0x0C)
//      stw     r7,4(r10)          n->next = p->freeList
//      lwz     r11,0(r3)          p->count              (+0x00)
//      addi    r6,r11,-1
//      stw     r10,12(r3)         p->freeList = n
//      stw     r6,0(r3)           p->count = count - 1
//      blr
//
// The `addi r9,r3,4` -- an ADDRESS that is stored, not loaded from -- is what
// identifies the layout. The list head is an embedded two-word node at +0x04,
// so `head.prev` is +0x04 and `head.next` is +0x08, and storing &p->head into
// the new first node's `prev` is the ordinary sentinel spelling. Reading the
// first node from +0x08 and writing the sentinel to +0x04 with one `addi`
// only works out if those two words are one object.
//
// Both `n` and `nx` are named locals: `n` is still live four instructions
// after the store that could alias it, and `nx` is used as a base immediately
// after being stored elsewhere. Neither is reloaded.
//
// Store order is source order -- 8, 0(nx), 4(n), 12, 0 -- with the count load
// hoisted up between the last two, which loads are free to do.
//
// Nothing is relocated; all 12 words are compared.

#include "types.h"

struct PNode
{
    /* 0x00 */ PNode* prev;
    /* 0x04 */ PNode* next;
};

ASSERT_OFFSET(PNode, next, 0x04);
ASSERT_SIZE(PNode, 8);

struct Pool
{
    /* 0x00 */ s32    count;
    /* 0x04 */ PNode  head;
    /* 0x0C */ PNode* freeList;
};

ASSERT_OFFSET(Pool, head,     0x04);
ASSERT_OFFSET(Pool, freeList, 0x0C);

void PoolReleaseFirst(Pool* p)
{
    PNode* n  = p->head.next;
    PNode* nx = n->next;

    p->head.next = nx;
    nx->prev     = &p->head;
    n->next      = p->freeList;
    p->freeList  = n;
    p->count--;
}
