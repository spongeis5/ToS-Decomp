// sub_82251AC8 -- zero one word, then tail-call memset over the 64 bytes that
// follow it. 24 bytes, 4 callers.
//
//      li      r11,0
//      li      r5,64            memset's n
//      stw     r11,0(r3)        the word at +0
//      li      r4,0             memset's c
//      addi    r3,r3,4          memset's dst
//      b       0x828A8C50       -> memset
//
// 828A8C50 is the CRT's hand-written memset -- byte head to word alignment,
// then a 16-byte unrolled body under `bdnz` (see src/g_memset_thunk.cpp,
// which forwards to the same address). So the last three arguments are set
// up and the branch is a TAIL CALL, not a loop.
//
// The `addi r3,r3,4` says the fill starts one word into the object, and the
// separate `stw` says that word is written on its own rather than being part
// of the fill -- so the source has a scalar member followed by a 64-byte
// array, and clears them with two statements. A single `memset(b, 0, 68)`
// would be one call with no store.

#include "types.h"

extern "C" void* __cdecl memset(void* dst, int c, size_t n);

struct Buf68
{
    /* 0x00 */ s32  count;
    /* 0x04 */ char data[64];
};

ASSERT_OFFSET(Buf68, data, 0x04);
ASSERT_SIZE(Buf68, 68);

void ClearBuf(Buf68* b)
{
    b->count = 0;
    memset(b->data, 0, 64);
}
