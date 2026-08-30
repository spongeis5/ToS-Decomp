// sub_826973C8 -- byte string compare. 44 bytes, 47 callers.
// NOT MATCHED: 9 of 11 words at /O2 /Os, 7 of 11 at /O2. See the bottom.
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
// Every one of the eleven instructions is otherwise correct at both levels,
// in the right order, at the right size. At /O2 four words differ (the two
// `lbz` destinations and the two instructions that read them); at /O2 /Os
// two differ (the `cmpwi`'s CR field and the `beqlr`'s).
//
// MEASURED THIS SESSION, and the first item is new evidence rather than one
// more failed shape:
//
// **The split is exactly the /Os bit, and it is reachable from SOURCE.**
// `#pragma optimize("s", on)` in the file, compiled at plain /O2, gives the
// /Os answer -- 9 of 11, r10/r9 with cr0. `#pragma optimize("t", on)`
// compiled at /O2 /Os gives the /O2 answer -- 7 of 11, r8/r7 with cr6. So
// the two properties are not merely correlated with a command-line flag that
// a sweep might have mis-covered; they are two consequences of one internal
// size-versus-speed decision, and a per-function source knob moves them
// together in both directions. `#pragma optimize` with "g" off is 104 B and
// 0 of 11; with "a" or "w" it is identical to the baseline.
//
// **THE CR SPLIT IS A PROPERTY OF THE LOOP, NOT OF "THE FIRST COMPARE".**
// Measured this session, and it is the mechanism the earlier note was
// missing. At /O2 /Os both fields appear in this one function -- the loop's
// TOP compare gets cr0 and the LATCH compare gets cr6 -- so "/Os prefers
// cr0" is not the rule either. A straight-line probe with three separate
// zero-compares gets cr0 for ALL of them at /Os and cr6 for all of them at
// /O2; a single-compare loop gets cr0 at /Os and cr6 at /O2. It is only when
// TWO compares live in one loop that /Os splits them, and it always splits
// them the same way round. Retail has cr6 for both, which is what /O2 does
// -- and /O2 is exactly where the byte registers go wrong. So the two
// properties are the same decision seen twice, and this function wants /O2's
// CR allocation with /Os's register allocation.
//
// **34 `#pragma optimize` COMBINATIONS, at both levels.** The earlier note
// records the single letters; the combinations behave identically and add
// one fact: every string whose effective size/speed bit is "s" -- "s",
// "gs", "ags", "gsw", "asw", "sy", and "t" turned OFF -- gives r10/r9 + cr0
// at BOTH command-line levels, and every string that is effectively "t" --
// "t", "gt", "agt", "gtw", "ty", and "s" off -- gives r8/r7 + cr6 at both.
// "a", "w" and "y" carry nothing in any combination, and any string with "g"
// off is 104 bytes and 0 of 11. There is no combination that separates the
// register allocation from the CR field.
//
// **41 source shapes compiled at both levels this session** -- a dozen of
// them repeats of the earlier 59, the rest chosen on two axes those did not
// vary: the SCOPE of the temporaries (declared at function scope,
// inside the loop body, one inside and one outside) and the EXIT STRUCTURE
// (`break` with the return after the loop, `continue`, a labelled `goto`
// out, `while (1)` with two returns) -- plus the second byte named in either
// declaration order, an inlined byte reader with the two calls either way
// round, `a[0]`/`b[0]`, a `struct Bytes` element, `__restrict`, a `long`
// return, three dead locals, the folded read `a[a - a]`, a separate pair of
// cursors, and a guard before the loop. Every 44-byte one of them is r8/r7 +
// cr6 at /O2 and r10/r9 + cr0 at /Os, without exception.
//
// The only shapes that reach r10/r9 at /O2 are the rotated ones -- `for (;;)`
// with two `if`s, and the labelled-goto exit -- and they peel the first
// iteration: 56 bytes at /Os and 64 at /O2, against the target's 44. That is
// the same peeling the earlier note reports.
//
// A MEASUREMENT THAT LOOKED LIKE MORE THAN IT WAS, recorded because it would
// have been believed: a sweep column reporting "the first compare's CR field"
// showed cr6 for those peeled shapes at BOTH levels, which reads as "peeling
// fixes the CR field too". It does not. In a peeled body the first compare in
// EMITTED order is the latch, not the NUL test, so the column was reporting a
// different instruction than in the compact body. Disassembling one in full
// settles it: at /Os the peeled loop is `cmpwi cr6,r3,0` at the latch and
// `cmpwi r10,0` -- still cr0 -- for the byte. Peeling moves the registers and
// leaves the CR field exactly where it was.
//
// **59 source shapes, at both levels** (11 of them also at /O1, which is the
// /Os answer): every one reproduces the same split, and every 44-byte one
// gives exactly r8/r7+cr6 at /O2 and r10/r9+cr0 at /Os. They were chosen to
// attack the two properties separately:
//
//   for the CR FIELD at /Os -- the byte un-named and spelled `*a` at both
//   uses (this is sub_825E35E0's naming lever, which moves cr6/cr0 there and
//   does nothing here), `(int)*a` casts, `a[0]`, `!c`, `0 == c`, `c < 1`,
//   a `zero` local, `(c | 0)`, `break` instead of the early return, the
//   guard written positively with the body in the `if` and the return in the
//   `else`, a `continue` form, a `goto` loop, `while (!d)`, both bytes named
//   in either declaration order, and `unsigned c` with casts;
//
//   for the GPR PAIR at /O2 -- an extra dead local, a dead pointer, a copy
//   of both parameters, an unused third parameter, a third parameter whose
//   use folds away, an index walk, post-increment dereference, `+= 1`,
//   `const u8*` versus `const char*` with `(u8)` casts, `__restrict` on both
//   parameters, a `struct Bytes` element type, and THREE MEMBER-FUNCTION
//   forms (`this` walked as `data`, as `&first`, and as a cast) -- the
//   member-function lever from sub_826C0FC8, which does not apply here;
//
//   and the FOLDED-READ lever that cracked the arena twins this session --
//   `a[a - a]`, `*a - *a + *a`, `*(a + (b - b))`, `c + (d - d)` in the NUL
//   test, `d + (c - c)` in the loop test, `if (a != a) return 0;` before and
//   inside the loop, and `a = a + (n - n);`. On the arena that lever moved an
//   `add`'s operand order because a read position survives the folding; here
//   nothing about register or CR-field allocation moves with it, in any of
//   the seven placements. That is worth recording as a LIMIT: the folded-read
//   lever reaches operand SELECTION, not register or CR-field ASSIGNMENT.
//
// Every shape that gets r10/r9 at /O2 does so by PEELING the first
// iteration, which costs 12 to 20 extra bytes and cannot be the target.
//
// **136 flag combinations beyond flagsweep's own 2304.** 44 further options
// from `cl /?` -- /GX /EHsc /EHa /GR /Zi /Z7 /GF /Gm /Oz /Oc /J /Zp1 /Zp2
// /Zp4 /Zp8 /Zp16 /Oi /Oi- /Ou /Ob0 /Ob1 /Ob2 /Ot /openmp /Zc:wchar_t- /GS
// /Gy- /H8 /vmg /vd0 /vd2 /Zl /TC /TP /analyze /Wall /W4 -- each crossed
// with /O2 and with /O2 /Os. Only /Ot moves anything, and only by cancelling
// /Os. (/Za, /GL, /RTCu, /u, /Zg and /bigobj do not compile here.)
//
// So the two properties are coupled to one optimisation decision, they
// disagree about which way that decision went, and neither the flag axis nor
// any source shape yet tried separates them. What has NOT been tried, and is
// where the next attempt should go, is a lever that changes what the
// register allocator SEES rather than what it is asked to compute -- the
// note in MATCHED.md that tools/permuter.py's mutations do not reach
// register allocation is about exactly this gap, and this function is the
// cleanest test case for a mutation that changes register pressure, because
// only two registers and one CR field are in question and every other word
// is already right.

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
