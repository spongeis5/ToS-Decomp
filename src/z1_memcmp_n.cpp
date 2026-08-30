#include "types.h"

// sub_825409E8 -- compare n bytes, returning the first difference. 64 B.
//
//      mr      r11,r3          p = a, the surviving induction pointer
//      li      r10,0           d = 0, materialised ABOVE the guard
//      cmpwi   cr6,r5,0        SIGNED, so `int n`
//      ble-    cr6,end         n <= 0: return 0
//      subf    r9,r3,r4        delta = b - a, ONCE, ahead of the loop
//  L:  lbzx    r7,r9,r11       *(p + delta)  ==  *q
//      lbz     r6,0(r11)       *p
//      subf    r4,r7,r6        rB - rA  ==  *p - *q
//      extsb   r10,r4          d = (char)(...)
//      cmpwi   cr6,r10,0
//      bne-    cr6,end
//      addic.  r5,r5,-1        --n, record form
//      addi    r11,r11,1       ++p
//      bgt+    L               while (n > 0)
// end: extsb   r3,r10          the char widened at the return
//      blr
//
// It sits 4 bytes after StrCompareNI, inside the string routines' translation
// unit (82540728..82540A28, all /O2), and it is the one member of that family
// with NO terminator test: only a count and a difference.
//
// THE BYTES ARE UNSIGNED AND THE DIFFERENCE IS A char. Both loads are plain
// lbz/lbzx with no sign extension, and the single extsb lands on their
// DIFFERENCE -- so the operands are `unsigned char` and the result is
// truncated to `char`, which is also why the return needs a second extsb of
// the same value. Sign-extending the operands instead is what StrCompareN
// does and it looks nothing like this.
//
// TWO WALKER LOCALS ARE THE WHOLE OF IT, and this is the lever recorded in
// src/g_memcmp_n.cpp from the other side, where it was the obstacle: "copying
// a and b into walker locals is a dead end -- MSVC strength-reduces the
// second pointer to a base difference and the loop becomes lbzx." That is
// precisely the shape here, so what defeated 82697748 produces 825409E8.
//
// Measured, 16 words each, all at plain /O2:
//
//   * incrementing the PARAMETERS a and b directly: no delta at all, two
//     walking pointers, 60 bytes and 0 of 15 words.
//   * subscripting both with one index, `a[i] - b[i]` (src/l45_cmp_bytes_n):
//     the right delta on the wrong array -- MSVC keeps `b + i` as the
//     induction pointer and reaches a through the delta, so the `mr`, the
//     delta's direction and the subf's operand order all invert together.
//     13 of 16, and `*(a + i) - *(b + i)` is the same 13.
//   * copying BOTH into walker locals: 16 of 16. The second local is the one
//     eliminated, so `p` (from a) is what survives in r11 -- which is the
//     choice the index form gets backwards.
//
// Insensitive to three things that were checked rather than assumed: whether
// `d` is declared before or after the walkers, whether `--n` precedes or
// follows the two increments, and whether `q` exists at all or `b` is reached
// as `b[p - a]`. All four spellings are 16 of 16. Folding the increments into
// the expression as `*p++ - *q++` is NOT the same -- that is 10 of 16.
//
// `subf rD,rA,rB` is rB - rA, so the difference is *p - *q, i.e. a - b.

int CompareBytesN(const u8* a, const u8* b, int n)
{
    const u8* p = a;
    const u8* q = b;
    char d = 0;

    while (n > 0)
    {
        d = (char)(*p - *q);

        if (d != 0)
            break;

        ++p;
        ++q;
        --n;
    }

    return d;
}
