#include "types.h"

// sub_82540838 -- in-place uppercase (strupr). 64 B.
//
//      lbz     r10,0(r3)       first byte
//      mr      r11,r3
//      cmplwi  cr6,r10,0       UNSIGNED test on the raw byte
//      beqlr   cr6             empty string: return s
//  L:  lbz     r10,0(r11)      <- RELOAD of the byte the peel already read
//      extsb   r10,r10         SIGNED inside the fold
//      cmpwi   cr6,r10,97      'a'
//      blt-    cr6,skip
//      cmpwi   cr6,r10,122     'z'
//      bgt-    cr6,skip
//      addi    r10,r10,-32     `- 32`, not `& ~0x20`
//      stb     r10,0(r11)
// skip:lbzu    r10,1(r11)      *++p
//      cmplwi  cr6,r10,0       UNSIGNED again
//      bne+    cr6,L
//      blr
//
// The peel plus a body whose bottom test is the loop's only test is the
// `if (*p != 0) do { ... } while (*++p != 0);` shape -- StrCat's guard with a
// do/while under it rather than an empty while.
//
// SIGNEDNESS IS SPLIT, exactly as in c_hash_upper (sub_8215A420): cmplwi on
// the raw byte at both zero tests, extsb + cmpwi inside the range check.
//
// r3 is never written, so s is the return value.

char* StrUpper(char* s)
{
    char* p = s;

    if (*p != 0)
    {
        do
        {
            char c = *p;
            if (c >= 'a' && c <= 'z')
                *p = (char)(c - 32);
        }
        while (*++p != 0);
    }

    return s;
}
