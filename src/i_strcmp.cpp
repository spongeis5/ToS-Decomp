#include "types.h"

// sub_82540878 -- plain string compare. 52 B, 13 callers.
//
// It sits 4 bytes before sub_825408B0 (StrCompareN, m_strcmp_n.cpp) and is
// the same routine without the count.
//
// IT AGREES WITH ITS NEIGHBOURS ON FLAGS, and informatively: it matches at
// /O2 and is 2 of 12 at /O2 /Os with a size difference, so it is
// flag-SENSITIVE and its /O2-only classification carries evidence rather
// than being one of the accessors that compile the same either way. That
// makes 82540878-825408B0 an agreeing adjacent pair with a 4-byte gap, and
// extends the string run to six functions and five consecutive agreeing
// pairs across 82540728..82540968.
//
//      addi    r10,r3,-1        a - 1
//      addi    r8,r4,-1         b - 1
//  L:  lbzu    r11,1(r10)       ca = *++a
//      lbzu    r9,1(r8)         cb = *++b
//      extsb   r11,r11
//      cmpwi   cr6,r11,0
//      beq-    cr6,end          ca == 0
//      extsb   r7,r9
//      cmpw    cr6,r11,r7
//      beq+    cr6,L            while (ca == cb)
// end: extsb   r10,r9
//      subf    r3,r10,r11       ca - cb
//      blr
//
// A do/while by the rule StrCopyN paid for: the loop top is a branch target
// reached by falling into it, with no peeled copy of the test in front. The
// biased pointers exist so lbzu can increment and load in one instruction.
//
// Both bytes are sign-extended before every use, so `char` is signed here, as
// in StrCompareN and StrCompareI. cb is extended TWICE -- once in the loop for
// the comparison and once at the end for the subtraction -- which is what the
// two-local shape gives.
//
// subf rD,rA,rB is rB - rA, so the result is ca - cb.
int StrCompare(const char* a, const char* b)
{
    char ca, cb;

    do
    {
        ca = *a++;
        cb = *b++;
        if (ca == 0)
            break;
    }
    while (ca == cb);

    return ca - cb;
}
