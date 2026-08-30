#include "types.h"

// sub_827FE818 -- 136 B, 24 callers. A BRIDGE between sub_827FE808
// (src/and_byte.cpp) and sub_827FE8A0 (src/m_cached_or_make.cpp).
//
//      mflr r12 ; stw r12,-8(r1) ; std r31,-16(r1) ; stwu r1,-96(r1)
//      mr      r31,r3
//      lwz     r3,20(r3)       h->d
//      lwz     r11,12(r3)  ; addic. r11,r11,-1 ; stw r11,12(r3)
//      bne-    +8          ; bl 0x82805D20
//      lwz     r3,16(r31)      h->c    ... same four instructions
//      lwz     r3,12(r31)      h->b    ... same four instructions
//      lwz     r3,8(r31)       h->a    ... same four instructions
//      epilogue
//
// Four copies of one idiom, and the idiom is the one src/m_node_destroy.cpp
// already reads out of sub_82805D20 itself: `addic. rD,rS,-1` decrements and
// sets CR0 in a single instruction, the store happens unconditionally, and
// `bne-` skips the destroy -- so it is `if (--n->refs == 0) DestroyNode(n);`
// written as one statement. The Node layout is that file's, measured there:
// refs at 0x0C, and 0x82805D20 is DestroyNode.
//
// The four fields are released at 0x14, 0x10, 0x0C, 0x08 -- DESCENDING, so
// the source is written in that order (call order is source order; there is
// nothing here for the scheduler to move, since each release depends on the
// last through r3). Reverse address order is also what a C++ destructor
// does to members declared 0x08..0x14, which is the likelier origin, but
// nothing in the bytes distinguishes a destructor from four statements --
// there is no vptr store and no `this` adjustment -- so this is written as
// what was measured.
//
// NO null test on any of the four: `lwz r3,n(r31)` goes straight into the
// refcount load. Whatever holds these four references guarantees them.
//
// Register discipline: one callee-saved register (r31, saved with std/ld
// rather than __savegprlr, so exactly one), holding the container across
// all four calls. `mr r31,r3` comes FIRST and the first field is then read
// through the original r3 -- r3 is free to be overwritten because it is
// also the argument register for the release.

// `Node` is src/m_node_destroy.cpp's struct, repeated field for field
// rather than re-invented: this file's four calls and that file's own
// recursive call are the SAME retail function at 0x82805D20, so a second
// name for the same type would make build.py's NAME DRIFT report say one
// function is two. Only `refs` is read here; `owner` and `parent` are
// carried across from the measurement there, not asserted again from these
// bytes.
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

struct RefHolder
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ Node* a;
    /* 0x0C */ Node* b;
    /* 0x10 */ Node* c;
    /* 0x14 */ Node* d;
};
ASSERT_OFFSET(RefHolder, a, 0x08);
ASSERT_OFFSET(RefHolder, b, 0x0C);
ASSERT_OFFSET(RefHolder, c, 0x10);
ASSERT_OFFSET(RefHolder, d, 0x14);

void DestroyNode(Node* n);

static void ReleaseRef(Node* n)
{
    if (--n->refs == 0)
        DestroyNode(n);
}

void ReleaseFour(RefHolder* h)
{
    ReleaseRef(h->d);
    ReleaseRef(h->c);
    ReleaseRef(h->b);
    ReleaseRef(h->a);
}
