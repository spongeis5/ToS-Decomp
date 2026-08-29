// sub_82540728 -- string length, 36 bytes, 37 callers.
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
// subf rD,rA,rB is rB - rA, so the result is p - s: the length.
// The empty-string case is peeled out ahead of the loop, which is why the
// first byte is loaded before r11 is set up.

int StrLen(const char* s)
{
    const char* p = s;
    if (*p != 0)
        while (*++p != 0)
            ;
    return (int)(p - s);
}
