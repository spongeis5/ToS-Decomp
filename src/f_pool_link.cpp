// sub_82600A08 -- take the head node off a free chain and splice it onto the
// front of a doubly-linked list, tracking a high-water mark. 84 B, 20 callers.
//
//      lwz     r10,12(r3)      p->free
//      addi    r11,r3,4        &p->head, used AS A NODE (the sentinel trick)
//      lwz     r9,4(r3)        p->head
//      lwz     r8,4(r10)       p->free->prev            saved before it dies
//      stw     r9,0(r10)       p->free->next = p->head
//      lwz     r7,12(r3)
//      stw     r11,4(r7)       p->free->prev = (Node*)&p->head
//      lwz     r6,4(r3)
//      lwz     r5,12(r3)
//      stw     r5,4(r6)        p->head->prev = p->free
//      lwz     r11,0(r3)       p->count
//      lwz     r4,24(r3)       p->peak
//      lwz     r10,12(r3)
//      stw     r10,4(r3)       p->head = p->free
//      addi    r11,r11,1
//      stw     r8,12(r3)       p->free = saved
//      cmpw    cr6,r4,r11
//      stw     r11,0(r3)       p->count = count + 1
//      bgelr   cr6
//      stw     r11,24(r3)      p->peak = p->count
//
// `p->free` is reloaded FOUR times and `p->head` twice: every store goes
// through a Node* the compiler cannot prove is disjoint from the Pool, so it
// re-reads after each one. That is the tell that the source spells the field
// out at each use rather than naming a local -- naming `Node* n = p->free;`
// collapses all four loads into one and changes the shape completely. The
// one value that IS named is the old `free->prev`, because the second store
// overwrites it.
//
// `addi r11,r3,4` takes the ADDRESS of the head pointer and stores it as a
// node's prev link, so the head slot doubles as a sentinel node whose `next`
// is the list head.
//
// The tail compares peak against the NEW count: `cmpw cr6,peak,count` with
// `bgelr` is `if (peak < count)` written with peak first, not `count > peak`.

#include "types.h"

struct Node
{
    /* 0x00 */ Node* next;
    /* 0x04 */ Node* prev;
};

ASSERT_OFFSET(Node, next, 0x00);
ASSERT_OFFSET(Node, prev, 0x04);

struct Pool
{
    /* 0x00 */ s32   count;
    /* 0x04 */ Node* head;
    /* 0x08 */ Node* tail;
    /* 0x0C */ Node* free;
    /* 0x10 */ s32   unk0010;
    /* 0x14 */ s32   unk0014;
    /* 0x18 */ s32   peak;
};

ASSERT_OFFSET(Pool, count, 0x00);
ASSERT_OFFSET(Pool, head, 0x04);
ASSERT_OFFSET(Pool, free, 0x0C);
ASSERT_OFFSET(Pool, peak, 0x18);

void PoolLink(Pool* p)
{
    Node* saved = p->free->prev;

    p->free->next = p->head;
    p->free->prev = (Node*)&p->head;
    p->head->prev = p->free;
    p->head = p->free;
    p->free = saved;
    p->count = p->count + 1;
    if (p->peak < p->count)
        p->peak = p->count;
}
