#include "types.h"

// sub_82156060 -- pop the head off the same list src/link_node.cpp
// (sub_82156050, immediately before this one) pushes onto: the `next` link is
// at +0x6C in both. 52 B, 7 callers.
//
//      lis     r10,-13057
//      mr      r11,r3          keep the head slot
//      lwz     r3,0(r3)        n = h->first        -- straight into r3
//      ori     r9,r10,52479    0xCCFFCCFF
//      cmplw   cr6,r3,r9       UNSIGNED: a pointer compare
//      bne-    cr6,take
//      li      r3,0
//      blr
// take:lwz     r10,108(r3)     n->next
//      li      r9,0
//      stw     r10,0(r11)      h->first = n->next
//      stw     r9,108(r3)      n->next = 0
//      blr
//
// 0xCCFFCCFF is the list's end-of-chain sentinel, not null -- the test is an
// equality against a literal, and the sentinel branch is the FALL-THROUGH, so
// it is written first (MATCHED.md, "branch polarity is source order").
//
// r3 is loaded once and never moved again, so the source names the loaded
// pointer in a local rather than respelling `h->first` (see the un-naming
// lever: a respelled expression would leave an `rlwinm rD,rS,0,0,31` behind).

struct Node108 { char unk0000[0x6C]; Node108* next; };
struct Head    { Node108* first; };
ASSERT_OFFSET(Node108, next,  0x6C);
ASSERT_OFFSET(Head,    first, 0x00);

Node108* PopFront(Head* h)
{
    Node108* n = h->first;
    if (n == (Node108*)0xCCFFCCFF)
        return 0;
    h->first = n->next;
    n->next  = 0;
    return n;
}
