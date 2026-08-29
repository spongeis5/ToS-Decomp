// sub_82157C58 -- zero one 516-byte array element by tail-calling memset.
// 20 bytes, 3 callers.
//
//      mulli   r11,r4,516
//      li      r5,516
//      li      r4,0
//      add     r3,r11,r3
//      b       0x828A8C50          -> memset
//
// The stride and the length are the SAME constant, so the call clears exactly
// one element and the element is 516 bytes. `mulli` rather than a shift/add
// chain because 516 is 4 * 129 and does not decompose cheaply -- the same
// reason sub_822D2450's 1856 stays a `mulli` at plain /O2.
//
// `add r3,r11,r3` with no displacement: the array starts at the pointer, so
// the parameter is the array itself rather than an object containing it.
//
// The tail branch is relocated, so 4 of 5 words are compared.

#include "types.h"

extern "C" void* __cdecl memset(void* dst, int c, size_t n);

struct Rec516
{
    /* 0x000 */ u8 unk0000[516];
};

ASSERT_SIZE(Rec516, 516);

void ClearRec(Rec516* recs, int i)
{
    memset(&recs[i], 0, 516);
}
