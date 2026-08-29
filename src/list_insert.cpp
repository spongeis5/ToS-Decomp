#include "types.h"

// sub_82600AD0 -- insert into a doubly linked list. 28 B, 5 callers.
//   lwz r11,0(r3) ; stw r3,4(r4) ; stw r11,0(r4)
//   lwz r10,0(r3) ; stw r4,4(r10) ; stw r4,0(r3) ; blr
//
// MATCHED, 7 of 7 words.
//
// The head field is loaded TWICE -- the compiler reloads it after the stores
// to r4 because it cannot prove they do not alias, and that reload is part
// of the target. Writing `head->next->prev = node` rather than naming the
// first load and reusing it is what keeps the reload: with `n->prev = node`
// the function is 24 bytes, one word short.
//
// THE ANSWER: THE LOAD'S POSITION IS SOURCE ORDER; THE STORES AROUND IT ARE
// NOT. This file used to write
//
//      node->prev = head;
//      node->next = head->next;
//
// and got `stw r3,4(r4)` before `lwz r11,0(r3)` -- five of seven words, the
// first two transposed. MSVC will not hoist a load above a store it cannot
// prove is to a different object, and `node->prev` at r4+4 could be the same
// address as `head->next` at r3+0. So the load stays where the source put
// it, and the ONLY way to get it first is to write the read first.
//
// The stores then come out in the opposite order from the source, which is
// the confusing part: the source reads head->next and stores it to node->next
// first, but `stw r3,4(r4)` is emitted between the load and its use, because
// it is the one instruction available to cover the load's latency. So the
// emitted store order is prev-then-next while the source order is
// next-then-prev.
//
// This is a third exception to "store order is source order" in MATCHED.md,
// and the cleanest one: the other two are scheduling ACROSS an address
// computation and across stores at distinct offsets. Here the gap being
// filled belongs to a LOAD, and the load itself is pinned by aliasing. So
// when a target's load sits between two stores, read the LOAD's position as
// source order and the stores' as free.
struct Link { Link* next; Link* prev; };
ASSERT_OFFSET(Link, next, 0x00);
ASSERT_OFFSET(Link, prev, 0x04);
void InsertAfter(Link* head, Link* node)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}
