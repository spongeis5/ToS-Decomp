// sub_825409E8 -- compare n bytes, returning the first difference. 64 B,
// 3 callers.
//
//      mr     r11,r3
//      li     r10,0
//      cmpwi  cr6,r5,0
//      ble-   cr6,end
//      subf   r9,r3,r4            b - a, ONCE, ahead of the loop
// L:   lbzx   r7,r9,r11           b[i] through that delta
//      lbz    r6,0(r11)           a[i]
//      subf   r4,r7,r6            a - b
//      extsb  r10,r4
//      cmpwi  cr6,r10,0
//      bne-   cr6,end
//      addic. r5,r5,-1
//      addi   r11,r11,1
//      bgt+   L
// end: extsb  r3,r10
//
// It sits 4 bytes after StrCompareNI, in the string routines' translation
// unit (82540728..82540968, all /O2), and it is the one member of that
// family with NO terminator test: only a count and a difference.
//
// THE BYTES ARE UNSIGNED AND THE DIFFERENCE IS A char.  Both loads are plain
// `lbz`/`lbzx` with no sign extension, and the single `extsb` lands on their
// DIFFERENCE -- so the operands are `unsigned char` and the result is
// truncated to `char`, which is also why the return needs a second `extsb`
// of the same value.  Sign-extending the operands instead is what
// StrCompareN does, and it looks nothing like this.
//
// MSVC keeps ONE walking pointer and reaches the second string through a
// loop-invariant delta computed before the loop; the source increments both.
//
// `subf rD,rA,rB` is rB - rA, so the difference is a - b.
//
// NOT MATCHED, 13 of 16, and the three wrong words are ONE decision: which
// of the two arrays becomes the induction pointer.  The target walks `a` and
// reaches b through the delta; ours walks `b` and reaches a.  Both compute
// a - b, so all three differences -- the `mr`, the delta's direction and the
// `subf`'s operand order -- move together and nothing else in the function
// changes.
//
// The shape is otherwise settled by measurement.  Incrementing BOTH pointers
// in source gives no delta at all: two walking pointers, 0 of 15 and a word
// shorter (which is the transform MATCHED.md records for an inlined
// hand-written strcmp, reached from the other side).  Subscripting both with
// one index and counting down with `n` is what produces the delta plus the
// countdown, exactly two induction variables as in the image.
//
// Ruled out for the remaining three words: reading the two bytes into
// temporaries in the other order (identical code -- the induction base is
// not chosen by source read order), and walking `a` while subscripting `b`
// with the index, which loses the delta entirely and scores 6 of 16.

#include "types.h"

int CompareBytesN(const u8* a, const u8* b, int n)
{
    char d = 0;
    int  i = 0;

    while (n > 0)
    {
        d = (char)(a[i] - b[i]);

        if (d != 0)
            break;

        --n;
        ++i;
    }

    return d;
}
