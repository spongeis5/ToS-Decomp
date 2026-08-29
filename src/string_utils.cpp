// String routines. sub_82540728 and sub_82540750.
//
// These two were matched separately, then merged into one translation unit on
// evidence rather than on the fact that they are both string functions:
//
//   * they sit 4 bytes apart -- strlen ends at 8254074C, strcopy starts at
//     82540750, one alignment word between them
//   * 12 functions call both of them
//
// Adjacency plus a shared caller set is the strongest same-object signal
// measured here: adjacent pairs that share a callee are 91.9% same-object
// against known library objects, against a 85.4% baseline (tools/segment.py
// --validate). It is evidence, not proof.
//
// Neither takes a struct, so there is no layout to assert.

#include "types.h"

// sub_82540728, 36 bytes, 37 callers.
//
//      lbz     r10,0(r3)       first byte
//      mr      r11,r3          keep the start
//      cmplwi  cr6,r10,0
//      beq-    cr6,end         empty string: skip the loop
//  L:  lbzu    r10,1(r11)      *++p
//      cmplwi  cr6,r10,0
//      bne+    cr6,L
//  end:subf    r3,r3,r11       r11 - r3
//      blr
//
// subf rD,rA,rB is rB - rA, so the result is p - s. The empty-string case is
// peeled out ahead of the loop, which is why the first byte is loaded before
// r11 is set up.
int StrLen(const char* s)
{
    const char* p = s;
    if (*p != 0)
        while (*++p != 0)
            ;
    return (int)(p - s);
}

// sub_82540750, 28 bytes, 49 callers.
//
//      addi    r9,r3,-1        d - 1
//      addi    r10,r4,-1       s - 1
//  L:  lbzu    r11,1(r10)      r11 = *++s
//      cmplwi  cr6,r11,0
//      stbu    r11,1(r9)       *++d = r11
//      bne+    cr6,L
//      blr
//
// r3 is never written, so the original destination is the return value --
// strcpy's contract. The biased pointers exist so the update forms can do the
// increment and the access in one instruction. `bne+` carries the TAKEN hint.
char* StrCopy(char* d, const char* s)
{
    char* p = d;
    while ((*p++ = *s++) != 0)
        ;
    return d;
}
