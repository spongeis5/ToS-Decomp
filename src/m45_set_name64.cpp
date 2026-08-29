// sub_82251AB0 -- clear a leading word and tail-call strncpy over the 64
// bytes after it. 20 bytes, 3 callers. Immediately before src/m20 (82251AC8),
// which is the same object with memset instead.
//
//      li      r11,0
//      li      r5,64
//      stw     r11,0(r3)
//      addi    r3,r3,4
//      b       0x828A9968          -> strncpy
//
// There is NO `li r4,0` here, and that is what identifies the callee. 828A9968
// copies from r4 until it sees a NUL, then pads the remainder of r5 with
// zeros -- strncpy, not memset -- so r4 is the incoming source string and
// passes straight through.
//
// Same object layout as src/m20_clear_and_zero64.cpp: a scalar at +0 written
// on its own, then 64 bytes filled by the call.
//
// The tail branch is relocated, so 4 of 5 words are compared.

#include "types.h"

extern "C" char* __cdecl strncpy(char* dst, const char* src, size_t n);

struct NameBuf
{
    /* 0x00 */ s32  count;
    /* 0x04 */ char name[64];
};

ASSERT_OFFSET(NameBuf, name, 0x04);
ASSERT_SIZE(NameBuf, 68);

void SetName(NameBuf* b, const char* s)
{
    b->count = 0;
    strncpy(b->name, s, 64);
}
