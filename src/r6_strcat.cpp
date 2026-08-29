#include "types.h"

// sub_82540798 -- strcat. 56 B, 7 callers.
//
// IT SITS 0 BYTES AFTER StrCopyN (sub_82540770, 40 B, ends at 82540798), which
// puts it inside the string run src/string_utils.cpp already holds --
// 82540728 StrLen, 82540750 StrCopy, 82540770 StrCopyN, 825408B0 StrCompareN,
// 825408F8 StrCompareI, 82540968 StrCompareNI, all /O2 only. It belongs in
// that translation unit; it is kept separate here only so two agents writing
// into src/ at once cannot lose each other's work.
//
//      lbz     r10,0(r3)       first byte of d
//      mr      r11,r3
//      cmplwi  cr6,r10,0
//      beq-    cr6,copy        empty destination: skip the measure
//  M:  lbzu    r10,1(r11)
//      cmplwi  cr6,r10,0
//      bne+    cr6,M
// copy:addi    r9,r11,-1       bias for stbu
//      addi    r10,r4,-1       bias for lbzu
//  C:  lbzu    r11,1(r10)
//      cmplwi  cr6,r11,0
//      stbu    r11,1(r9)
//      bne+    cr6,C
//      blr
//
// The two halves are StrLen's body without the `subf` and StrCopy's body
// verbatim -- the same peeled empty-string test in front of the measure, the
// same biased pointers in the copy.
//
// It is the HAND-WRITTEN measure, not the strlen intrinsic: the -1 is folded
// into the destination register (`addi r9,r11,-1`), where the intrinsic keeps
// the length in its own register and follows with `rlwinm rX,rY,0,0,31`.
//
// r3 is never written, so d is the return value -- strcat's contract.

char* StrCat(char* d, const char* s)
{
    char* p = d;
    if (*p != 0)
        while (*++p != 0)
            ;
    while ((*p++ = *s++) != 0)
        ;
    return d;
}
