#include "types.h"

// sub_8214C578 -- find a node by key, walking a singly-linked list. 52 B,
// 11 callers.
//
//      cmplwi  cr6,r3,0
//      beq-    cr6,ret0
//      lwz     r3,12(r3)        n = l->head
//      cmplwi  cr6,r3,0
//      beq-    cr6,ret0
//  L:  lwz     r11,16(r3)       n->key
//      cmplw   cr6,r11,r4
//      beqlr   cr6              found: r3 IS n, so it is already the result
//      lwz     r3,0(r3)         n = n->next
//      cmplwi  cr6,r3,0
//      bne+    cr6,L
// ret0:li      r3,0
//      blr
//
// The walker lives in r3 the whole way, which is why the hit needs no move:
// `beqlr` returns the node it is standing on. All three failure paths merge
// on one `li r3,0`.
//
// The loop is ROTATED -- the `n == 0` test is peeled out ahead of the top and
// a second copy sits at the bottom -- so this is a `for`/`while`, not the
// do/while that StrCopyN needed. That peeled copy is the diagnostic.
//
// cmplw/cmplwi throughout: the key compare is unsigned.
struct FindNode
{
    /* 0x00 */ FindNode* next;
    /* 0x04 */ char      unk0004[12];
    /* 0x10 */ u32       key;
};
ASSERT_OFFSET(FindNode, next, 0x00);
ASSERT_OFFSET(FindNode, key,  0x10);

struct FindList
{
    /* 0x00 */ char      unk0000[12];
    /* 0x0C */ FindNode* head;
};
ASSERT_OFFSET(FindList, head, 0x0C);

FindNode* FindByKey(FindList* l, u32 key)
{
    FindNode* n;

    if (l == 0)
        return 0;

    for (n = l->head; n != 0; n = n->next)
        if (n->key == key)
            return n;

    return 0;
}
