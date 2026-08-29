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

struct E { char pad00[24]; void* v; };          // +0x18
struct D { char pad00[4];  E*    e; };          // +0x04
struct C { char pad00[12]; D*    d; };          // +0x0C
struct B { char pad00[76]; C*    c; };          // +0x4C
struct A { char pad00[56]; B*    b; };          // +0x38

void* GetThroughChain(A* a)
{
    return a->b->c->d->e->v;
}
