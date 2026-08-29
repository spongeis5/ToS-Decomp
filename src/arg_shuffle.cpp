#include "types.h"

// sub_8215E5B0 -- argument reshuffle into a tail call. 28 B, 26 callers.
//
//      lwz     r10,0(r3)     the load happens FIRST, into a scratch
//      mr      r11,r5
//      mr      r5,r4
//      li      r6,0
//      mr      r4,r10
//      mr      r3,r11
//      b       0x82602EA0    = TrackOrAdd (src/m_track_or_add.cpp)
//
// The moves are a permutation, so the call is TrackOrAdd(c, this->first, b, 0).
//
// MATCHED, and the answer is that THE RESULT IS FORWARDED, not discarded.
// TrackOrAdd returns `void*`; this thunk returns it too. That single change
// -- `return TrackOrAdd(...)` in a function whose return type is `void*`,
// instead of a `void` function calling it -- is the whole match.
//
// WHY IT MOVES ANYTHING AT ALL. The permutation is a copy CYCLE:
// r3 <- r5, r5 <- r4, r4 <- *r3. Something has to be staged through a
// scratch, and which thing decides all five words. With r3 dead after the
// branch, MSVC breaks the cycle at r4 -- `mr r11,r4 ; lwz r4,0(r3)` -- and
// loads straight into the argument register. With the result forwarded, r3
// is LIVE OUT across the tail call, so the sequencer stages the LOAD instead
// -- `lwz r10,0(r3)` leading, then a clean three-move permutation with r11
// as the temporary. Same seven instructions either way; only the choice of
// what gets staged differs, and only the return type reaches it.
//
// This is the same lever MATCHED.md records for sub_826A3648, reached from a
// different direction: there a constructor and `operator=` fixed a hoist that
// eleven rearrangements could not, and "what the two that work have in common
// is that r3 is live out". Here nothing about the object is involved -- it is
// the callee's own result being returned -- and r3 being live out is again
// the whole of it. **When a function ends in a tail call and the register
// shuffle in front of it is wrong, try forwarding the result before touching
// the argument list.**
//
// EIGHTEEN SPELLINGS OF THE ARGUMENTS ARE BYTE-IDENTICAL, which is what
// makes the return type the answer rather than one shape among many: the
// dereference named in a local and not named; `this->first` spelled out; all
// three arguments named as locals in forward and reverse declaration order;
// the object pointer named before and after the loaded value; the free
// function taking `A0*`; a const view of `this` for the load; a non-virtual
// member call on `c` instead of a free call; the fourth argument as a null
// pointer rather than an `int`; and a callee returning `int` whose result is
// dropped. Every one of them is 1 of 6, with exactly the five words above
// wrong. Only forwarding the result is 6 of 6.
//
// The claim this file used to make -- that naming the dereference was "the
// whole match" -- was wrong: it is measured at 1 of 6 above, alongside the
// unnamed form it was said to beat.
//
// Two other things about it are worth keeping: its recorded size is 156
// bytes because one .pdata row covers six frameless thunks (see FINDINGS.md
// 7q), and objdiff scored it 12.1% for that reason alone.
struct A0
{
    void* first;
    void* Forward(void* b, void* c);
};
ASSERT_OFFSET(A0, first, 0x00);

void* TrackOrAdd(void* key, void* unused, void* ctx, void* extra);

void* A0::Forward(void* b, void* c)
{
    void* v = first;
    return TrackOrAdd(c, v, b, 0);
}
