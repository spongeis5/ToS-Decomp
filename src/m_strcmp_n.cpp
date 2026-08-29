#include "types.h"

// sub_825408B0 -- bounded string compare. 68 B, 21 callers.
//
//      li      r10,0           i = 0
//      addi    r9,r3,-1        a - 1
//      addi    r8,r4,-1        b - 1
//  L:  lbzu    r11,1(r9)       ca = *++a
//      addi    r10,r10,1       ++i
//      lbzu    r7,1(r8)        cb = *++b
//      extsb   r11,r11
//      cmpwi   cr6,r11,0
//      beq-    cr6,end         ca == 0
//      extsb   r6,r7
//      cmpw    cr6,r11,r6
//      bne-    cr6,end         ca != cb
//      cmpw    cr6,r10,r5
//      blt+    cr6,L           while (i < n)
// end: extsb   r10,r7
//      subf    r3,r10,r11      ca - cb
//      blr
//
// The body is entered with no test, so it is a do/while and a count of 0
// still compares one character. Both bytes are sign-extended before every
// use, which is `char` on this compiler, not `unsigned char`: an unsigned
// comparison would have used cmplw and no extsb at all.
//
// `subf rD,rA,rB` is rB - rA, so the result is ca - cb and not the other way
// round -- worth stating because getting it backwards still compiles and
// still returns a plausible-looking int.
int StrCompareN(const char* a, const char* b, int n)
{
    int i = 0;
    char ca, cb;

    do
    {
        ca = *a++;
        ++i;
        cb = *b++;
        if (ca == 0)
            break;
        if (ca != cb)
            break;
    }
    while (i < n);

    return ca - cb;
}
