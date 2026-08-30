#include "types.h"

// sub_82589FF8 -- reset a circular list to empty. 52 B, 4 callers.
//
//      lwz     r10,8(r4)        last
//      addi    r11,r4,4         the anchor: &head->first
//      lwz     r9,4(r4)         first
//      li      r8,0
//      li      r3,0
//      stw     r9,0(r10)        last->next = first
//      lwz     r7,4(r4)         RELOADED -- the stores through the node
//      lwz     r6,8(r4)         pointers may alias the head, so MSVC
//      stw     r6,4(r7)         re-reads both fields: first->prev = last
//      stw     r11,8(r4)        head->last  = anchor
//      stw     r11,4(r4)        head->first = anchor
//      stw     r8,12(r4)        count = 0
//      blr
//
// No locals: the head's fields are re-read through the head each time,
// which is what produces the reloads. Return value is 0. The head arrives
// in r4 -- an unused leading parameter, as in w4_vec5_init.

struct Node
{
    /* 0x00 */ Node* next;
    /* 0x04 */ Node* prev;
};

struct Ring
{
    /* 0x04 */ char unk0000[4];
    /* 0x04 */ Node* first;
    /* 0x08 */ Node* last;
    /* 0x0C */ s32   count;
};

ASSERT_OFFSET(Ring, first, 4);
ASSERT_OFFSET(Ring, last, 8);
ASSERT_OFFSET(Ring, count, 12);

int ClearRing(void* self, Ring* h)
{
    h->last->next = h->first;
    h->first->prev = h->last;
    h->last  = (Node*)&h->first;
    h->first = (Node*)&h->first;
    h->count = 0;
    return 0;
}
