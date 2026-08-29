// sub_82790F80 -- virtual call on the SECOND argument, first ignored.
// 20 B, 4 callers.
//
//   lwz   r11,8(r4)
//   mr    r3,r4
//   lwz   r11,0(r11)
//   mtctr r11
//   bctr
//
// Register discipline, read off the listing:
//
//  * r3 arrives and is OVERWRITTEN without being read -- the first parameter
//    is never used.  That is what makes this a member function whose body
//    touches only its argument rather than a one-argument forwarder.
//  * the callee receives r3 = r4, i.e. the object is its own `this`, so the
//    dispatch table lives at +8 OF THAT OBJECT and slot 0 takes it back.
//  * the `mr` sits BETWEEN the two dependent loads: it is the one instruction
//    available to cover the first load's latency, not a source statement
//    between them.
//
// Slot index is 0/4 = 0.
//
// /Os DECIDED THIS ONE.  At /O2 the five instructions are right and in the
// right order, and the two words after the first load differ only in
// register name -- the vtable slot is loaded into a fresh r10 and mtctr'd
// from it, where the target reuses r11.  3 of 5 at /O2, 5 of 5 at /O2 /Os
// with no change to the source.  That is the fresh-versus-reused signature,
// not a source shape.

#include "types.h"

struct Node;

struct NodeVT
{
    void (*Run)(Node* self);
};

struct Node
{
    /* 0x00 */ char    unk0000[0x08];
    /* 0x08 */ NodeVT* vt;
};
ASSERT_OFFSET(Node, vt, 0x08);

struct Runner
{
    void Run(Node* n);
};

void Runner::Run(Node* n)
{
    n->vt->Run(n);
}
