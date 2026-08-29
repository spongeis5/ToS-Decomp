// sub_826A32E8 -- case-folding string hash, walked BACKWARDS.
// 60 bytes, 33 callers.
//
//      cmplwi  cr6,r4,0
//      beq-    cr6,done            zero length: the seed passes through
//      mtctr   r4                  counted loop, trip count = n
//  L:  addi    r4,r4,-1            --n  (n is also the index)
//      lbzx    r11,r3,r4           c = s[n]
//      cmpwi   cr6,r11,65          'A'
//      blt-    cr6,skip
//      cmpwi   cr6,r11,90          'Z'
//      bgt-    cr6,skip
//      addi    r11,r11,32          fold to lower case
//  skip:
//      mulli   r10,r5,33           h * 33
//      xor     r5,r10,r11          h = h * 33 ^ c
//      bdnz+   L
//  done:
//      mr      r3,r5
//      blr
//
// djb2 with xor instead of add, seeded by the third argument, and ASCII-only
// case folding done inline rather than through tolower(). The index counts
// DOWN, so the last character is hashed first.
//
// `mtctr`/`bdnz` is the counted loop: the compiler proved the loop runs
// exactly n times and kept n live only as the index. `cmpwi` (signed) against
// a byte that arrived through `lbzx` (zero-extended) is the ordinary
// int-promoted compare.

#include "types.h"

u32 HashLower(const u8* s, u32 n, u32 h)
{
    while (n != 0)
    {
        int c;
        --n;
        c = s[n];
        if (c >= 'A' && c <= 'Z')
            c += 32;
        h = h * 33 ^ c;
    }
    return h;
}
