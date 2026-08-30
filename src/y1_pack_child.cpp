#include "types.h"

// sub_826969B8 -- delegate to the child's own writer with 48 bytes reserved,
// range-check its answer, and report either the whole size or just the
// header. 140 B.  Bridge between Acc_826969B0 and Acc_82696A48.
//
//      lwz  r3,24(r3) ; addi r30,r5,-48 ; mr r5,r30
//      lwz  r11,0(r3) ; lwz r10,24(r11) ; bctrl        child->vt slot 6
//      cmpwi cr6,r3,0 ; bge- ; li r3,-1 ; b out
//      cmpw  cr6,r3,r30 ; bgt+ BACKWARD into that same li
//      lwz  r11,24(r31) ; addi r10,r31,48 ; cmplw ; bne-
//      li r11,0 ; addi r3,r3,48 ; stw r11,28(r31) ; b out
//      stw r3,28(r31) ; li r3,48
//
// The second guard branching BACKWARD into the first one's `li r3,-1` is the
// flat two-guard form (MATCHED.md, sub_821675B8 read the other way round --
// there all the guards branched FORWARD to a shared tail and needed
// nesting).  `avail` is held in a non-volatile across the call, so it is a
// named local.  The two signed compares are s32; the pointer compare is
// cmplw.

struct Child;

struct ChildVT
{
    void* slot0;
    void* slot1;
    void* slot2;
    void* slot3;
    void* slot4;
    void* slot5;
    s32 (*pack)(Child*, void*, s32);
};

struct Child
{
    /* 0x00 */ ChildVT* vt;
};
ASSERT_OFFSET(Child, vt, 0x00);

struct Node
{
    /* 0x00 */ u8     unk0000[0x18];
    /* 0x18 */ Child* child;
    /* 0x1C */ s32    packed;
    /* 0x20 */ u8     unk0020[0x10];
    /* 0x30 */ Child  inner;
};
ASSERT_OFFSET(Node, child,  0x18);
ASSERT_OFFSET(Node, packed, 0x1C);
ASSERT_OFFSET(Node, inner,  0x30);

s32 PackChild(Node* n, void* dst, s32 size)
{
    s32 avail = size - 48;
    s32 r = n->child->vt->pack(n->child, dst, avail);

    if (r < 0)
        return -1;
    if (r > avail)
        return -1;

    if (n->child == &n->inner)
    {
        n->packed = 0;
        return r + 48;
    }
    n->packed = r;
    return 48;
}
