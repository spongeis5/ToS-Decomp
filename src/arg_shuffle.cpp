#include "types.h"

// sub_8215E5B0 -- argument reshuffle into a tail call. 28 B, 26 callers.
//
//      lwz     r10,0(r3)     the load happens FIRST, into a scratch
//      mr      r11,r5
//      mr      r5,r4
//      li      r6,0
//      mr      r4,r10
//      mr      r3,r11
//      b       0x82602EA0
//
// The moves are a permutation, so the call is Callee(c, a->first, b, 0).
//
// NAMING THE DEREFERENCE IS THE WHOLE MATCH. Written as
// `Callee(c, first, b, 0)` the compiler frees r4 first and loads straight
// into it -- `mr r11,r4 ; lwz r4,0(r3) ; ...`, five words wrong. Named in a
// local, the load is pulled to the front while r4 is still occupied and has
// to go via a scratch, which is the target's `lwz r10,0(r3)` leading.
//
// The local controls WHEN the load happens; the register copy is a
// consequence. The same knob points the other way in a_vcall4_or_neg1.cpp,
// where deleting a local was what matched.
//
// This sat in src/attempts.txt as one of six stalls, described as "register
// assignment across an argument permutation" -- which was the symptom, not
// the cause. Two other things about it were also wrong: its recorded size is
// 156 bytes because one .pdata row covers six frameless thunks (see
// FINDINGS.md 7q), and objdiff scored it 12.1% for that reason alone.
struct A0
{
    void* first;
    void Forward(void* b, void* c);
};
ASSERT_OFFSET(A0, first, 0x00);

void Callee(void*, void*, void*, int);

void A0::Forward(void* b, void* c)
{
    void* v = first;
    Callee(c, v, b, 0);
}
