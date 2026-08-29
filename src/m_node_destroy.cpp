#include "types.h"

// sub_82805D20 -- release a node, and its parent if that was the last
// reference. 108 B, 276 callers. RECURSIVE: the `bl` at +0x38 is to itself.
//
//      mflr r12 ; stw r12,-8(r1) ; std r31,-16(r1) ; stwu r1,-112(r1)
//      mr      r31,r3
//      lwz     r3,8(r3)        parent
//      cmplw   cr6,r3,r31
//      beq-    cr6,rest        parent == self: nothing to release
//      cmplwi  cr6,r3,0
//      beq-    cr6,rest
//      lwz     r11,12(r3)
//      addic.  r11,r11,-1      decrement AND set CR0 in one instruction
//      stw     r11,12(r3)
//      bne-    rest
//      bl      0x82805D20      itself
// rest:stw     r31,80(r1)      a local holding `n` ...
//      addi    r4,r1,80        ... whose ADDRESS is passed
//      lwz     r3,4(r31)
//      bl      0x82805A40
//      mr      r4,r31
//      lwz     r3,4(r31)
//      bl      0x82805740
//      epilogue
//
// `addic. rD,rS,-1` followed by `bne-` is `if (--x == 0)`: the decrement
// sets CR0 itself, so the compare is free. Written as two statements --
// `x = x - 1; if (x == 0)` -- MSVC still folds it, but written as
// `if (x - 1 == 0) { x = x - 1; ... }` it does not, because the store then
// has to happen on both paths.
//
// The `stw r31,80(r1)` / `addi r4,r1,80` pair is a LOCAL whose address is
// taken. Passing `n` directly would put it in r4 with no stack traffic at
// all, so the second routine takes a pointer TO the pointer -- it can clear
// or replace the caller's copy.
struct Node
{
    /* 0x00 */ char   unk0000[4];
    /* 0x04 */ void*  owner;
    /* 0x08 */ Node*  parent;
    /* 0x0C */ s32    refs;
};
ASSERT_OFFSET(Node, owner, 0x04);
ASSERT_OFFSET(Node, parent, 0x08);
ASSERT_OFFSET(Node, refs, 0x0C);

void DetachNode(void* owner, Node** slot);
void FreeNode(void* owner, Node* n);

void DestroyNode(Node* n)
{
    Node* p = n->parent;
    if (p != n && p != 0)
    {
        if (--p->refs == 0)
            DestroyNode(p);
    }

    Node* held = n;
    DetachNode(n->owner, &held);
    FreeNode(n->owner, n);
}
