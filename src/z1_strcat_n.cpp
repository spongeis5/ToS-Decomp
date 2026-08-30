#include "types.h"

// sub_825407D0 -- bounded string append (strncat). 100 B.
//
//      mr      r11,r3          p = d
//      cmpwi   cr6,r5,0        SIGNED test on n, so `int`
//      beqlr   cr6             n == 0: return d untouched
//      lbz     r10,0(r3)       first byte of d
//      cmplwi  cr6,r10,0
//      beq-    cr6,copy        empty destination: skip the measure
//  M:  lbzu    r10,1(r11)
//      cmplwi  cr6,r10,0
//      bne+    cr6,M
// copy:lbz     r10,0(r4)       <- PEELED copy of the loop's test
//      cmplwi  cr6,r10,0
//      stb     r10,0(r11)
//      beqlr   cr6             the source's NUL was copied: done
//      addic.  r5,r5,-1        --n, record form
//      addi    r11,r11,1       ++p
//      addi    r4,r4,1         ++s
//      beq-    term            n ran out
//  L:  lbz     r10,0(r4)       <- the loop's own copy of the test
//      cmplwi  cr6,r10,0
//      stb     r10,0(r11)
//      bne+    cr6,0x82540804  back to the addic.
//      blr
// term:li      r10,0
//      stb     r10,0(r11)
//      blr
//
// The measure is StrLen's body without the subf and StrCat's verbatim: the
// same peeled empty-string test in front of it.
//
// The copy is a ROTATED `while`, not a do/while -- there are two copies of
// the lbz/cmplwi/stb test, one ahead of the loop and one at the bottom, which
// is exactly what StrCopyN's comment says a do/while never produces.
//
// r3 is never written, so d is the return value -- strncat's contract.

char* StrCatN(char* d, const char* s, int n)
{
    char* p = d;

    if (n == 0)
        return d;

    if (*p != 0)
        while (*++p != 0)
            ;

    while ((*p = *s) != 0)
    {
        ++p;
        ++s;
        if (--n == 0)
        {
            *p = 0;
            break;
        }
    }

    return d;
}
