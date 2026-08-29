// sub_827B7660 -- the file name inside a path: scan back for the last
// separator. 84 B, 4 callers.
//
//      mr      r11,r3
// S:   lbz     r10,0(r11)
//      addi    r11,r11,1
//      cmplwi  cr6,r10,0
//      bne+    cr6,S
//      subf    r11,r3,r11
//      addi    r11,r11,-1
//      rotlwi. r11,r11,0
//      beqlr
// L:   lbzx    r10,r11,r3
//      add     r9,r11,r3
//      extsb   r10,r10
//      cmpwi   cr6,r10,92
//      beq-    cr6,hit
//      cmpwi   cr6,r10,47
//      beq-    cr6,hit
//      addic.  r11,r11,-1
//      bne+    L
//      blr
// hit: addi    r3,r9,1
//      blr
//
// The first seven instructions are the INLINED strlen INTRINSIC, not a
// hand-written loop: the giveaway is that the -1 is not folded into the
// argument register but left as a separate `addi`, followed by the explicit
// `rlwinm rD,rS,0,0,31` zero-extension of size_t -- here in its record form,
// because the same instruction also supplies the `len == 0` test.
//
// The backward scan is a do/while by the StrCopyN rule: its top is a branch
// target reached by FALLING INTO it, with no peeled copy of the test ahead.
// The peeled test that does exist belongs to the `len == 0` guard, which
// returns the string unchanged -- r3 is still the argument at that point,
// which is why the guard costs no `li`.
//
// The scan starts at s[len], the terminator, and stops before s[0]: a leading
// separator is therefore not a hit, and neither branch is a bug.
//
// `add r9,r11,r3` computes `s + i` inside the loop for a value only the hit
// path uses -- the source forms `s + i + 1` in the return, and the load is a
// subscript (`lbzx`), so the pointer is not a named induction variable.
// Its operand order follows the add rule: rA holds the operand whose source
// read comes LATER, and `s + i` reads s first.
//
// `extsb` before both compares: the character is signed.

#include "types.h"
#include <string.h>

char* AfterLastSeparator(char* s)
{
    size_t i = strlen(s);

    if (i == 0)
        return s;

    do
    {
        char c = s[i];

        if (c == '\\' || c == '/')
            return s + i + 1;
    }
    while (--i != 0);

    return s;
}
