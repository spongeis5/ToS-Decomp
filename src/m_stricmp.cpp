#include "types.h"

// sub_825408F8 and sub_82540968 -- case-insensitive string compare, plain and
// bounded. 108 B / 124 B, 20 and 14 callers.
//
// They belong with StrLen/StrCopy/StrCopyN in string_utils.cpp by every
// signal except one: they are 0x1A8 bytes past StrCopyN with sub_825408B0
// (StrCompareN) in between, and nothing establishes that the run is one
// object rather than two. Kept separate because a wrong split costs only
// duplicated effort while a wrong merge invents a type identity that
// compiles fine and is very hard to notice.
//
//      lbz     r9,0(r3)          <- LOOP TOP, reached by falling into it
//      extsb   r11,r9
//      cmpwi   cr6,r11,65        'A'
//      blt-    cr6,skip
//      cmpwi   cr6,r11,90        'Z'
//      bgt-    cr6,skip
//      addi    r11,r11,32
//      extsb   r9,r11
// skip:lbz     r10,0(r4)
//      ... the same six instructions again for the second character ...
//      cmpwi   cr6,r11,0
//      beq-    cr6,end
//      cmpw    cr6,r11,r9
//      beq+    cr6,loop
// end: subf    r3,r10,r11
//      blr
//
// A do/while, by the rule StrCopyN paid for: the loop top is a branch target
// reached by fall-through with no peeled copy of the test in front of it.
//
// The fold is written twice in the emitted code, which is an inlined helper
// rather than a macro -- the same six instructions, the same registers, and
// `+ 32` rather than `& ~0x20`, so the source really does add.
//
// Every character is sign-extended before every use, so `char` is signed
// here, as it was in StrCompareN.
static char LowerAscii(char c)
{
    if (c >= 'A' && c <= 'Z')
        c = (char)(c + 32);
    return c;
}

int StrCompareI(const char* a, const char* b)
{
    char ca, cb;

    do
    {
        ca = LowerAscii(*a++);
        cb = LowerAscii(*b++);
        if (ca == 0)
            break;
    }
    while (ca == cb);

    return ca - cb;
}

// sub_82540968 -- the same, with a count. The only additions are `li r8,0`
// ahead of the loop, `addi r8,r8,1` inside it and `cmpw cr6,r8,r5` at the
// bottom, exactly as StrCompareN adds them to StrCompare.
int StrCompareNI(const char* a, const char* b, int n)
{
    int i = 0;
    char ca, cb;

    do
    {
        ca = LowerAscii(*a++);
        cb = LowerAscii(*b++);
        ++i;
        if (ca == 0)
            break;
        if (ca != cb)
            break;
    }
    while (i < n);

    return ca - cb;
}
