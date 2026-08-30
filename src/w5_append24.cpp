#include "types.h"

// sub_82589FC8 -- append a node to a circular list whose head stores the
// anchor as a pointer-to-next-field. 44 B, 4 callers.
//
//      lwz     r8,28(r3)        old tail
//      mr      r10,r3
//      addi    r9,r3,24         the anchor: &head->first
//      addi    r11,r4,4         &node->next -- the intrusive anchor form
//      li      r3,0
//      stw     r9,4(r4)         node->next = &head->first
//      stw     r8,8(r4)         node->prev = old tail
//      stw     r11,28(r10)      head->last = &node->next
//      lwz     r7,8(r4)         re-read node->prev (may alias)
//      stw     r11,0(r7)        old tail->next = &node->next
//      blr
//
// `last` always points at a next-FIELD, empty or not; `first` is untouched.

struct Node
{
    /* 0x00 */ Node* next;
    /* 0x04 */ Node* prev;
};

struct Ring24
{
    /* 0x18 */ char unk0000[24];
    /* 0x18 */ Node* first;
    /* 0x1C */ Node* last;
};

ASSERT_OFFSET(Ring24, first, 24);
ASSERT_OFFSET(Ring24, last, 28);

int Append(Ring24* h, Node* n)
{
    Node* old_last = h->last;
    n->next = (Node*)&h->first;
    n->prev = old_last;
    h->last = (Node*)&n->next;
    n->prev->next = (Node*)&n->next;
    return 0;
}

// NEAR-MISS. Register-number allocation: the target reads head->last into
// r8 and saves the head into r10; ours starts at r10/r11. Store order and
// every computation agree. Both flag levels and a named-local spelling
// leave the assignment unmoved.
