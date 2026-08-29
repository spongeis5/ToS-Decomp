#include "types.h"

// sub_82156050 -- push onto a singly linked list. 16 B, 7 callers.
//   lwz r11,0(r3) ; stw r11,108(r4) ; stw r4,0(r3) ; blr
struct Node108 { char unk0000[0x6C]; Node108* next; };
struct Head    { Node108* first; };
ASSERT_OFFSET(Node108, next,  0x6C);
ASSERT_OFFSET(Head,    first, 0x00);
void PushFront(Head* h, Node108* n) { n->next = h->first; h->first = n; }
