// sub_8215E5B0 -- argument reshuffle into a tail call, 28 bytes, 26 callers.
//
//      lwz     r10,0(r3)       t = *a
//      mr      r11,r5          save arg3 before r3 is overwritten
//      mr      r5,r4           new arg3 = old arg2
//      li      r6,0            new arg4 = 0
//      mr      r4,r10          new arg2 = t
//      mr      r3,r11          new arg1 = old arg3
//      b       0x82602EA0
//
// The register moves form a permutation, and the r11 temporary exists only
// because arg1 and arg3 swap. So the call is
//
//      Callee(c, *a, b, 0)     from    f(a, b, c)
//
// The branch target is relocated: 6 of 7 words are compared.

struct A { void* first; };

void Callee(void*, void*, void*, int);

void Forward(A* a, void* b, void* c)
{
    Callee(c, a->first, b, 0);
}
