// sub_8225D5D0 -- publish a level to two globals, when the flag and the
// argument agree. 80 B, 4 callers.
//
//      lis    r10,-32092
//      lbz    r11,56(r10)          the byte at 82A40038
//      cmplwi cr6,r11,0
//      bne-   cr6,A
//      clrlwi r9,r4,24 ; cmplwi ; beq- cr6,W
//      cmplwi cr6,r11,0 ; beqlr cr6
// A:   clrlwi r11,r4,24 ; cmplwi ; beqlr cr6
// W:   lis    r9,-32101 ; li r11,1 ; addi r8,r9,-19728    = 829AB2F0
//      stb    r11,56(r10)
//      stfs   f1,1448(r8)
//      stb    r4,1452(r8)
//      stfs   f1,1444(r8)
//
// THE ARGUMENT REGISTERS NAME THE SIGNATURE.  r3 is never read and f1 holds
// the float: on this ABI every parameter consumes one slot, so a leading
// `float` takes slot 0 (f1, r3 unused) and the byte lands in r4 as slot 1.
// A member function would have to read r3.
//
// The predicate is ONE expression, two `&&` conjuncts joined by `||`, and
// the BLOCK ORDER is what says so.  As two separate `if` guards it compiles
// to the same ten instructions with the two blocks exchanged and the first
// branch inverted -- 10 of 20, every difference a branch target.  Written as
// one condition:
//
//      if ((g_flag != 0 && on == 0) || (on != 0 && g_flag == 0)) return;
//
//  * `bne-` sends the flag-SET case out of line to finish the first
//    conjunct, so the fall-through belongs to the SECOND conjunct.  Two
//    statements give the first conjunct the fall-through instead.
//  * on the fall-through the second conjunct is entered at its own first
//    term, `on`, and only then re-tests the flag already in r11 -- the
//    `cmplwi cr6,r11,0 ; beqlr` that no if/else spelling emits.
//  * in the out-of-line block the second conjunct is provably false, so it
//    folds and only `on == 0` remains.
//
// So the body runs exactly when the flag and the argument agree.
//
// The stores are two streams interleaved one for one by dual issue: integer
// (the flag, then the byte at 1452) and float (1448, then 1444), each in its
// own source order.  Reading the merged order back as source order is the
// documented way to lose this one.
//
// NOT MATCHED, and the residue is exactly two BLOCKS EXCHANGED.  10 of the
// 15 non-relocated words agree (20 compared, 5 relocated); every instruction
// is present and correct, and the five that differ are the entry branch's
// polarity plus the two blocks it chooses between:
//
//      target   [test flag; bne- C1] [C2] [C1] [work]
//      ours     [test flag; beq- C2] [C1] [C2] [work]
//
// where C1 is `on == 0 -> return`, the tail of the first conjunct, and C2 is
// the whole second conjunct.  The store block is byte-identical either way.
//
// Measured and rejected, each making it worse rather than differently wrong:
//
//   * the two conjuncts as two separate `if` statements -- identical code,
//     same 10 of 15, so `||` is not what decides the layout here;
//   * the conjuncts in the opposite source order -- 0 of 15, the entry test
//     becomes `on`;
//   * a nested if/else on the flag -- 2 words SHORTER, because inside the
//     `flag == 0` arm MSVC folds the second conjunct's `flag == 0` away.
//     That fold is the diagnostic: the target's redundant `cmplwi cr6,r11,0
//     ; beqlr` proves the second test survives, which only the two-conjunct
//     spelling produces;
//   * /O2 /Os -- 0 of 15, the whole body shifts.
//   * the same two conjuncts written with implicit bool conversions,
//     `(g_level_dirty && !on) || (on && !g_level_dirty)` -- a different AST
//     for the same tests, and BYTE-IDENTICAL, same 10 of 15.
//   * the POSITIVE form, `if (!(...)) { work }` with the four stores inside
//     the guard instead of an early return -- byte-identical, same 10 of 15.
//     So the polarity of the outer statement carries nothing here.
//   * a ternary with the false arm laid out first,
//     `if (g_level_dirty == 0 ? (on != 0 && g_level_dirty == 0) : (on == 0))`
//     -- chosen because that IS the target's block order, and much worse:
//     76 bytes, 0 of 19, one flag compare instead of two and `bnelr` where
//     the image has a pair. A `?:` cannot produce this function: the SECOND
//     `cmplwi cr6,r11,0` in the image is the fingerprint of two conjuncts.
//
// So the source shape is settled by the instructions and only the block
// placement is open, which is the class of difference MATCHED.md warns is
// sometimes not source-reachable.
//
// THE BLOCK ORDER, named precisely, so the next reader is not re-deriving
// it. Write T1 = `flag != 0`, T2 = `on == 0`, T3 = `on != 0`, T4 =
// `flag == 0`; the source is (T1 && T2) || (T3 && T4).
//
//      target   [T1] [T3 T4] [T2] [work]      entry bne- to T2
//      ours     [T1] [T2] [T3 T4] [work]      entry beq- to T3
//
// MSVC lays blocks in the order the expression creates them -- T1, T2, T3,
// T4 -- which is ours, and no rewriting of one `||` expression reorders
// that without also changing which term is tested at the entry (the
// conjuncts swapped is 0 of 15, with `on` at the entry).
//
// One thing in the target is NOT explained by any layout of ours, and is
// the strongest clue left: the target's T2 block ends `beqlr` and falls
// straight into the work, so its FALSE edge goes to the work rather than to
// conjunct 2 -- which means MSVC folded conjunct 2 away on the `flag != 0`
// edge. Yet it did NOT fold the `cmplwi cr6,r11,0 ; beqlr` in the T4 block,
// which is always true on its only predecessor. A shape that gets one fold
// without the other is what is missing.

#include "types.h"

struct LevelState
{
    /* 0x0000 */ char unk0000[1444];
    /* 0x05A4 */ f32  a;
    /* 0x05A8 */ f32  b;
    /* 0x05AC */ u8   which;
};
ASSERT_OFFSET(LevelState, a,     1444);
ASSERT_OFFSET(LevelState, b,     1448);
ASSERT_OFFSET(LevelState, which, 1452);

extern u8         g_level_dirty;      /* 82A40038 */
extern LevelState g_level_state;      /* 829AB2F0 */

void SetLevel(float v, u8 on)
{
    if ((g_level_dirty != 0 && on == 0) || (on != 0 && g_level_dirty == 0))
        return;

    g_level_dirty = 1;
    g_level_state.which = on;

    g_level_state.b = v;
    g_level_state.a = v;
}
