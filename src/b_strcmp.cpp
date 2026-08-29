// sub_826973C8 -- byte string compare. 44 bytes, 47 callers.
// NOT MATCHED: 9 of 11 words at /O2 /Os. See the bottom of this file.
//
//      mr      r11,r3          keep the first pointer; r3 becomes the result
//  L:  lbz     r10,0(r11)
//      lbz     r9,0(r4)
//      cmpwi   cr6,r10,0
//      subf    r3,r9,r10       r10 - r9, i.e. *a - *b
//      beqlr   cr6             a hit NUL: return the difference
//      addi    r11,r11,1
//      addi    r4,r4,1
//      cmpwi   cr6,r3,0
//      beq+    cr6,L           equal so far: keep going
//      blr
//
// Both bytes arrive through `lbz`, so the subtraction is on ZERO-EXTENDED
// bytes -- unsigned char semantics, which is what strcmp requires and what
// rules out signed char (that would need extsb before the subtraction).
//
// The difference is computed every iteration and tested at the BOTTOM while
// the NUL test sits in the middle, so this is a do/while with an early
// return, not `while (*a && *a == *b)`.
//
// The byte value goes into an `int` local because `*a == 0` on a `u8`
// compiles to `cmplwi` -- the target's `cmpwi` is the signed compare an int
// gets.
//
// ---------------------------------------------------------------------
// WHAT IS LEFT, and it is one decision the compiler makes two ways at once:
//
//      /O2       cmpwi cr6,r8,0 / beqlr cr6    correct CR field, WRONG GPRs
//                lbz r8 / lbz r7               (target uses r10 / r9)
//
//      /O2 /Os   cmpwi r10,0 / beqlr           correct GPRs, WRONG CR field
//                lbz r10 / lbz r9              (cr0, target uses cr6)
//
// The two properties are coupled to the optimisation level and no flag
// separates them. tools/flagsweep.py --full compiled 2304 distinct
// combinations against this source: 864 score 9/11, 1440 score 7/11, none
// scores more. Undocumented spellings tried by hand on top of that
// (/Ou-, /Oz, /Oc, /J, /Zp1, /Og /Oi /Ot /Oy) all score 7/11.
//
// Source shape does not move it either. Eleven loop shapes were compiled at
// both levels -- do/while, for(;;) with two returns, while(1) with breaks,
// post-increment deref, both bytes in locals, an explicit local copy of the
// first pointer, an unused third parameter, and three shapes written to put
// the `d == 0` test EARLIER in source order than the `c == 0` test on the
// theory that /Os hands cr0 to whichever comparison it meets first. Every
// shape that keeps the loop unpeeled gives r8/r7 at /O2 and cr0 at /Os;
// every shape that gets r10/r9 at /O2 does so by PEELING the first
// iteration, which costs 12 to 20 extra bytes and cannot be the target.
//
// So under /Os the compiler assigns cr0 to the comparison feeding the
// conditional return and cr6 to the one feeding the loop-back branch,
// independently of the order they are written in. That is a scheduling or
// CR-allocation property, the same class of thing as the six stalls in
// MATCHED.md, and nothing reachable from source order or flags changes it.

#include "types.h"

int StrCmp(const u8* a, const u8* b)
{
    int c, d;
    do
    {
        c = *a;
        d = c - *b;
        if (c == 0)
            return d;
        ++a;
        ++b;
    } while (d == 0);
    return d;
}
