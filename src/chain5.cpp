// sub_821636A8 -- five dependent loads, 24 bytes, 26 callers.
//
//      lwz     r11,56(r3)
//      lwz     r10,76(r11)
//      lwz     r9,12(r10)
//      lwz     r8,4(r9)
//      lwz     r3,24(r8)
//      blr
//
// Chosen deliberately: every instruction depends on the one before it, so
// there is NO scheduling freedom for the compiler to disagree with us about.
// Both previous stalls (82806FD0, 826C1480) were functions with independent
// loads and stores the scheduler could permute; this one has none.

#include "types.h"

struct E { /* 0x18 */ char unk0000[0x18]; void* v; };
struct D { /* 0x04 */ char unk0000[0x04]; E*    e; };
struct C { /* 0x0C */ char unk0000[0x0C]; D*    d; };
struct B { /* 0x4C */ char unk0000[0x4C]; C*    c; };
struct A { /* 0x38 */ char unk0000[0x38]; B*    b; };

ASSERT_OFFSET(E, v, 0x18);
ASSERT_OFFSET(D, e, 0x04);
ASSERT_OFFSET(C, d, 0x0C);
ASSERT_OFFSET(B, c, 0x4C);
ASSERT_OFFSET(A, b, 0x38);

void* GetThroughChain(A* a)
{
    return a->b->c->d->e->v;
}
