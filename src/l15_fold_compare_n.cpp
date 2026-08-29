// sub_8215AA28 -- case-insensitive compare of n bytes. 88 B, 4 callers.
//
//      li     r11,0
//      cmplwi cr6,r5,0
//      beq-   cr6,zero
// L:   lbzx   r10,r11,r3        a[i]
//      lbzx   r9,r11,r4         b[i]
//      extsb  r8,r10 ; extsb r7,r9
//      srawi  r6,r8,1 ; srawi r10,r7,1
//      rlwinm r9,r6,0,26,26     & 0x20
//      rlwinm r6,r10,0,26,26
//      or     r10,r9,r8
//      or     r9,r6,r7
//      cmpw   cr6,r10,r9
//      bne-   cr6,diff
//      addi   r11,r11,1
//      cmplw  cr6,r11,r5
//      blt+   cr6,L
// zero:li     r3,0 ; blr
// diff:subf   r3,r9,r10 ; blr
//
// The fold is the branchless lower-case of src/c_hash_upper.cpp read in the
// other direction: bit 6 is set for both letter cases, so `(c >> 1) & 0x20`
// is 0x20 exactly for letters and OR-ing it in forces lower case.
//
// SIGNEDNESS, which decides the instruction count here as it did there: the
// bytes arrive through `lbzx` (unsigned) and are immediately `extsb`'d, and
// the shift is `srawi` -- ARITHMETIC.  That is the sign-extended form, and it
// is what makes MSVC keep `(c >> 1) & 0x20` in that order rather than
// reassociating it to `(c & 0x40) >> 1`, which is what the all-unsigned
// spelling collapses to.  The index and the count are `cmplw`, unsigned.
//
// `subf r3,r9,r10` is `fa - fb` by the subf rule: subf rD,rA,rB computes
// rB - rA.
//
// The loop is the ordinary rotated `for`: the count test is peeled out in
// front and the increment plus `blt+` closes the bottom.  The zero return is
// shared by the peeled guard and the fall-out, so it is written once, last.

#include "types.h"

static int FoldLower(char c)
{
    return c | ((c >> 1) & 0x20);
}

int CompareFoldedN(const u8* a, const u8* b, u32 n)
{
    u32 i;

    for (i = 0; i < n; i++)
    {
        int fa = FoldLower((char)a[i]);
        int fb = FoldLower((char)b[i]);

        if (fa != fb)
            return fa - fb;
    }

    return 0;
}
