#include "types.h"

// sub_8215E5B0 -- argument reshuffle into a tail call. 28 B, 26 callers.
//   lwz r10,0(r3) ; mr r11,r5 ; mr r5,r4 ; li r6,0
//   mr  r4,r10 ; mr r3,r11 ; b 0x82602EA0
// The moves are a permutation, so the call is Callee(c, a->first, b, 0).
struct A0
{
    void* first;
    void Forward(void* b, void* c);
};
ASSERT_OFFSET(A0, first, 0x00);

void Callee(void*, void*, void*, int);

void A0::Forward(void* b, void* c) { Callee(c, first, b, 0); }
