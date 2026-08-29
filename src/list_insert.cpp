#include "types.h"

// sub_82600AD0 -- insert into a doubly linked list. 28 B, 5 callers.
//   lwz r11,0(r3) ; stw r3,4(r4) ; stw r11,0(r4)
//   lwz r10,0(r3) ; stw r4,4(r10) ; stw r4,0(r3) ; blr
// The head field is loaded TWICE -- the compiler reloads it after the stores
// to r4 because it cannot prove they do not alias, and that reload is part
// of the target.
struct Link { Link* next; Link* prev; };
ASSERT_OFFSET(Link, next, 0x00);
ASSERT_OFFSET(Link, prev, 0x04);
void InsertAfter(Link* head, Link* node)
{
    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
}
