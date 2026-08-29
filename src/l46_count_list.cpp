// sub_825476F8 -- count the nodes on a circular list. 64 B, 3 callers.
//
//      cmplwi cr6,r4,0
//      bne-   cr6,go
//      li     r3,37
//      blr
// go:  lwz    r11,352(r3)
//      addi   r9,r3,352
//      li     r10,0
//      cmplw  cr6,r11,r9
//      beq-   cr6,done
// L:   lwz    r11,0(r11)
//      addi   r10,r10,1
//      cmplw  cr6,r11,r9
//      bne+   cr6,L
// done:stw    r10,0(r4)
//      li     r3,0
//
// A CIRCULAR list with a sentinel: the loop stops when the walk comes back
// to `this + 352`, and there is NO null test anywhere.  By the sentinel rule
// that means the head is a whole node embedded in the object rather than a
// bare link -- `&o->head` is a plain `addi r9,r3,352`, where a cast of a
// Link member to a node would cost the null-keeps-null adjustment.
//
// The count is written through the out parameter and the status is the
// return: 37 when the out pointer is null, 0 otherwise.  The failure test is
// first and its `li r3,37` is private, not shared with the success path's
// `li r3,0`.
//
// The loop advances BEFORE counting, so an empty list gives 0 and the
// sentinel is never counted.

#include "types.h"

struct ListNode
{
    /* 0x00 */ ListNode* next;
};

struct NodeOwner
{
    /* 0x0000 */ char     unk0000[0x160];
    /* 0x0160 */ ListNode head;
};
ASSERT_OFFSET(NodeOwner, head, 0x160);

int CountNodes(NodeOwner* o, int* out)
{
    if (out == 0)
        return 37;

    int n = 0;
    ListNode* p = o->head.next;

    while (p != &o->head)
    {
        p = p->next;
        ++n;
    }

    *out = n;
    return 0;
}
