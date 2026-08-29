// sub_82673EC8 -- decide whether two packed ids are "related": compare their
// 16-bit group halves, and either compare two 5-bit sub-fields crosswise or
// fall back to a bit-matrix lookup. 144 B, 5 callers.
// r3 = bool out, r4 = the matrix owner, r5 = a, r6 = b.
//
//   xor    r11,r5,r6 ; rlwinm r10,r11,0,0,15 ; bne- -> tail
//   rlwinm r11,r5,0,0,15                     ; beq- -> tail
//   rlwinm r11,r6,27,5,31   = b >> 5   (unsigned: rotate 27, mask 5..31)
//   xor / rlwinm r9,r10,0,22,26  = & 0x3E0   (bits 5..9)
//   bne- L2 ; li r11,0 ; stb ; blr
// L2: same crosswise ; beq+ BACKWARD into the zero store ; li r11,1 ; stb
// tail: clrlwi 27 twice ; addi r9,r11,13 ; rlwinm 2,0,29 ; lwzx ; and ;
//       addic/subfe  -- branchless `!= 0`
//
// THE GUARDS BRANCH BACKWARD into a shared `li r11,0 ; stb ; blr`, which per
// src/i_sum_parts.cpp (sub_821675B8) is the signature of FLAT guards written
// as `if (...) { *out = false; return; }` rather than a nested positive path.
// A nested `&&` would send both forward to a common tail instead.
//
// `addi r9,r11,13` before a shift-by-2 is NOT a constant in the source: per
// the sub_821A6B38 lever MSVC folds a member array's byte offset into the
// index when the stride is a shift, so the table is at byte 52 with 4-byte
// elements.
//
// NEAR MISS: 16 of 36 words, 136 bytes against 144.  Every word from
// 82673EC8 to 82673F0C is already identical -- the two 16-bit guards, the
// crosswise mask, the shared zero block planted between the tests and the
// first test inverted to `bne-` to jump over it.  TWO WORDS ARE MISSING and
// everything after them is displaced by the size difference:
//
//   82673F10  the second guard is IF-CONVERTED.  We emit
//             `addic/subfe ; stb` -- that is, `*out = (y != 0)` -- where the
//             target keeps `cmplwi ; beq+ BACKWARD ; li 1 ; stb`.
//   82673F44  the target copies the AND result into itself, `addi r11,r11,0`,
//             before the branchless `!= 0` in the tail.  Ours goes straight
//             into `addic`.
//
// The if-conversion is the whole of it, and nothing reaches it.  Measured,
// so it is not re-tried: flat guards, if/elseif/else, a `goto` to a shared
// block and a `goto` INTO the first block, `x || y` around one store, an
// `&&` expression, a bool local, a nested positive path with the zero store
// last, an unrolled two-pass loop, `*out = false` before the guards, and the
// guards as `while`; the destination as `u8*`, `volatile bool*` and `bool&`;
// the whole body, the inner block and a member of Matrix as bool-returning
// helpers; `s32` table elements; the row and column in locals; and `> 0`
// instead of `!= 0` in the tail AND in the guards.  Then all 72 flag
// combinations `tools/flagsweep.py` builds: 44 give 16 of 36 and 28 give
// 1 of 36, so the optimisation level is exhausted as well.
//
// The one shape that scores higher is not plausible source and is recorded
// only so nobody re-derives it: making the two masked values `s32` and
// writing `if (!(x > 0))` reaches 24 of 35 at 140 bytes, because the signed
// `> 0` branchless form (`neg`/`andc`/`rlwinm`) happens to be the same
// LENGTH as the target's branchy one.  It still if-converts; it just
// mis-matches in place instead of shifting the tail.

#include "types.h"

struct Matrix
{
    /* 0x00 */ char unk0000[52];
    /* 0x34 */ u32  bits[32];
};
ASSERT_OFFSET(Matrix, bits, 52);

void PairRelated(bool* out, const Matrix* m, u32 a, u32 b)
{
    if (((a ^ b) & 0xFFFF0000) == 0 && (a & 0xFFFF0000) != 0)
    {
        if ((((b >> 5) ^ a) & 0x3E0) == 0) { *out = false; return; }
        if ((((a >> 5) ^ b) & 0x3E0) == 0) { *out = false; return; }
        *out = true;
        return;
    }
    *out = (m->bits[a & 31] & (1u << (b & 31))) != 0;
}
