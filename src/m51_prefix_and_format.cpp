// sub_821F6250 -- write one literal character, then format a number after it.
// 28 bytes, 3 callers.
//
//      li      r11,86              'V'
//      lis     r10,-32244
//      mr      r5,r4               the incoming number becomes argument 3
//      stb     r11,0(r3)
//      addi    r3,r3,1
//      addi    r4,r10,-6528        -> 820BE680, which holds "%d"
//      b       0x828A9CB8          -> sprintf
//
// 828A9CB8 spills r5 through r10 to the caller's parameter save area with
// `std` before doing anything else, which is the varargs prologue, so it
// takes a format string -- and the string at 820BE680 is "%d", read out of
// the image. The literal 86 is 'V'.
//
// `mr r5,r4` before the branch is the argument moving from slot 2 to slot 3
// to make room for the format string, and `addi r3,r3,1` is the buffer
// advancing past the character already written.
//
// The tail branch is relocated, so 6 of 7 words are compared.

#include "types.h"

extern "C" int __cdecl sprintf(char* buf, const char* fmt, ...);

void WriteVersionTag(char* buf, int n)
{
    buf[0] = 'V';
    sprintf(buf + 1, "%d", n);
}
