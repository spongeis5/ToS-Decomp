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
//
// NOT MATCHED.  0 of 22 words, and ours is 3 words LONGER, because MSVC
// applies the LOOP-INVARIANT-DELTA transform to the two subscripts:
//
//      subf r7,r4,r3           a - b, once, before the loop
//   L: lbzx r10,r7,r11         a[i] as *(p + delta)
//      lbz  r9,0(r11)          b[i] through a walking pointer
//      addi r8,r8,1            ... and a THIRD induction variable for the
//      addi r11,r11,1              count, because the bound stayed an index
//
// Three induction variables where the target has one, which also frees r3
// and r4 as scratch -- the target keeps both base pointers live for the two
// `lbzx` all the way down.  This is the same transform MATCHED.md records
// for an inlined hand-written strcmp, where the fix was to call the real
// strcmp; there is no library function for this fold.
//
// Ruled out, all identical or worse: /O2 /Os (still the delta form, 0 of
// 22), and a guarded do/while instead of the rotated `for` (0 of 22).  The
// fold itself is not in question -- the eleven instructions from `extsb` to
// the second `or` are byte-identical in every attempt; only the addressing
// and the induction variables differ.

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
