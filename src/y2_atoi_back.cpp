// sub_82540B18 -- decimal string to integer, walked from the LAST character
// backwards with a running place value. 228 bytes, 3 callers.
//
// It sits 0x1B8 past the string block whose five routines are already
// matched (StrLen 82540728 .. StrCompareNI 82540968), so this is the same
// translation unit's `atoi`.
//
//      lbz   r10,0(r4) ; mr r11,r4 ; cmplwi ; beq-
//      lbzu  r10,1(r11) ; cmplwi ; bne+          <- rotated `while (*p) p++;`
//      addi  r11,r11,-1                          <- then step back onto the
//                                                   last character
//
// The `-1` folded into the pointer rather than a length being formed is the
// hand-written walk, not the `strlen` intrinsic (MATCHED.md: the intrinsic
// keeps the count in a register and follows with `rotlwi rD,rS,0`).
//
// Everything after is one loop that MSVC UNROLLED BY TWO, and the unroll is
// the compiler's, not the source's:
//
//      subf r9,r4,r11 ; addi r9,r9,1 ; cmpwi cr6,r9,2 ; blt-     count < 2
//      subf r9,r4,r11 ; addi r9,r9,-1 ; rlwinm r9,r9,31,1,31 ; addi r9,r9,1
//      mtctr r9                                                 (n-1)/2 + 1
//
// -- the classic MSVC by-two trip count, two private accumulators (r5 and
// r6, summed with `add r11,r5,r6` at the end) and a residual copy of the
// body after the `bdnz`. r3 is the residual's own contribution and is
// `li r3,0` in the entry block, which is also the null guard's return.
//
// Inside the body the place value is multiplied twice per iteration by the
// shift/add form `((p*4)+p)*2`, and the two digits are scaled by `place` and
// `place*10` respectively, so the source multiplies by 10 once per character.
//
// `extsb` on each byte says the character is SIGNED -- a plain `char`, whose
// default on this target is signed -- and the pointer comparison that ends
// the loop is `cmplw`, unsigned, which is what a pointer compare always is.
//
// THE NULL GUARD IS AN `if`/`else` WITH THE FAILURE ARM WRITTEN FIRST, and
// the whole function turned on that. The entry block wanted is
//
//      mr r4,r3 ; cmplwi cr6,r3,0 ; li r3,0 ; beq- cr6,<epilogue>
//
// -- the compare reads r3 while it still holds `s`, and the zero lands in
// the compare's delay slot. Four spellings were measured at /O2:
//
//   if (s == 0) return 0; ... int sum = 0;         4 of 57, 236 bytes -- the
//                                                    guard gets its own
//                                                    epilogue, because MSVC
//                                                    knows r3 is already 0
//                                                    on that path and needs
//                                                    no `li` at all
//   const char* p = s; int sum = 0; if (p != 0)    52 of 57 -- two `mr`s
//   int sum = 0; if (s != 0) { ... }               55 of 57 -- correct
//                                                    structure, but the
//                                                    initialiser puts `li`
//                                                    BEFORE the compare and
//                                                    the compare then has to
//                                                    read the copy in r4
//   int sum; if (s == 0) sum = 0; else { ... }     57 of 57
//
// This is the same lever src/y2_range_lookup.cpp records, reached from a
// zero rather than a -1: the arm of an `if`/`else` written FIRST is the one
// whose constant is hoisted between the compare and its branch, and an
// equivalent single-assignment-before-the-`if` does not place it there.
//
// The two remaining accumulators are the unroller's and are initialised in
// the loop preheader, so the source has ONE `sum` and it is zeroed here.
//
// `/O2 /Os` does not unroll at all -- 26 words against 57 and less than half
// the size -- so the level is not in question.

#include "types.h"

int ParseDecimal(const char* s)
{
    int sum;
    if (s == 0)
    {
        sum = 0;
    }
    else
    {
        sum = 0;

        const char* p = s;
        while (*p != 0)
            p++;
        p--;

        int place = 1;
        while (p >= s)
        {
            sum += (*p - '0') * place;
            place *= 10;
            p--;
        }
    }
    return sum;
}
