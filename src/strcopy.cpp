// sub_82540750 -- byte copy to a NUL, 28 bytes, 49 callers.
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
// strcpy's contract. The pre-decrement then update-form load/store is what
// MSVC emits for the *p++ = *q++ idiom; the biased pointers exist so the
// update forms can do the increment and the access in one instruction.
//
// The `bne+` carries the TAKEN hint, i.e. the loop is assumed to iterate.

char* StrCopy(char* d, const char* s)
{
    char* p = d;
    while ((*p++ = *s++) != 0)
        ;
    return d;
}
