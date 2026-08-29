// sub_82581448 -- the wide-character twin of StrCopyN in
// src/string_utils.cpp (sub_82540770). 40 bytes, 3 callers.
//
//      addi   r9,r3,-2         biased destination
//      addi   r10,r4,-2        biased source
//  L:  cmpwi  cr6,r5,0
//      addi   r5,r5,-1
//      beqlr  cr6
//      lhzu   r11,2(r10)
//      cmplwi cr6,r11,0
//      sthu   r11,2(r9)
//      bne+   cr6,L
//      blr
//
// Every element of the byte version is here with the width changed: the bias
// is -2 instead of -1 so `lhzu`/`sthu` can increment and access in one
// instruction, the compare-then-decrement pair is a post-decrement in the
// condition, and the copy happens BEFORE the NUL test so the terminator is
// written out.
//
// IT IS A do/while, for the same reason and with the same tell: the loop top
// is the `n` test and it is reached by FALLING INTO it, with no peeled copy
// ahead of the two `addi`s. `while (n--)` rotates and peels; a do/while is
// the one loop MSVC never rotates.
//
// `cmpwi` on the count is signed, `cmplwi` on the character is unsigned --
// the two compares name their own types.
//
// Nothing is relocated: 10 of 10 words are compared.

#include "types.h"

u16* WStrCopyN(u16* d, const u16* s, int n)
{
    u16* p = d;
    u16 c = 0;
    do
    {
        if (n-- == 0)
            break;
        c = *s++;
        *p++ = c;
    }
    while (c != 0);
    return d;
}
