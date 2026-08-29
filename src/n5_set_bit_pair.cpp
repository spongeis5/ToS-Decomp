// sub_825FF468 -- set the same bit in two adjacent byte masks. 36 B,
// 4 callers.
//
//      li      r10,1
//      lbz     r7,0(r3)
//      lbz     r6,1(r3)
//      slw     r8,r10,r4        1 << n, computed once
//      or      r5,r7,r8
//      or      r4,r6,r8
//      stb     r5,0(r3)
//      stb     r4,1(r3)
//      blr
//
// THE TWO BYTE FIELDS ARE SIGNED, AND THAT IS THE WHOLE FUNCTION. With `u8`
// fields this compiles to the SAME NINE INSTRUCTIONS in the SAME ORDER with
// the same operand shapes, and eight of the nine words differ -- every one of
// them only a register NAME:
//
//      want  li r10,1 / lbz r7 / lbz r6 / slw r8,r10,r4 / or r5,r7,r8
//            / or r4,r6,r8 / stb r5 / stb r4
//      u8    li r11,1 / lbz r10 / lbz r9 / slw r8,r11,r4 / or r7,r10,r8
//            / or r6,r9,r8  / stb r7 / stb r6
//
// The `u8` allocation is a clean descending run r11,r10,r9,r8,r7,r6 in
// definition order. The image's is r10,r7,r6,r8,r5,r4 -- it SKIPS r11 and
// r9, which is what two extra virtual registers ahead of the run look like.
// Those two are the sign extensions: `s8 |= int` promotes with a sign
// extend, and MSVC then proves the extend dead because only the low byte is
// ever stored, so no `extsb` is emitted -- but the nodes have already been
// numbered and the allocation has already moved down. 9 of 9 at /O2.
//
// This was NOT reachable from either of the two axes usually tried first, and
// both were exhausted before the type was suspected:
//
//   * 28 SOURCE SHAPES at both levels. Sixteen of them are byte-identical to
//     the `u8` version at 1 of 9 -- a named local for the mask, the mask
//     spelled inline twice, `const int`, `u32`/`u32 n`, explicit `a = a | m`,
//     `m | a`, a member function, a raw `u8*`, `v[0]`/`v[1]` on an array
//     member, `((u8*)p)[0]`, two separate `u8*` parameters, a reference
//     parameter, `int one = 1`, a `Bit(n)` helper, an inlined one-pointer
//     helper, an inlined two-pointer helper, an inlined helper nested two
//     levels deep, and a dead extra copy of the mask. Four more are the wrong
//     SIZE: `u8 m = (u8)(1 << n)` and naming the two loads in locals both add
//     a word, and every loop spelling (for/do-while, array or pointer) is
//     40 or 44 bytes because the trip count survives.
//   * ALL 2304 FLAG COMBINATIONS that flagsweep.py --full builds from the
//     compiler's own option list. Every one scores 1 of 9 at 36 bytes. So
//     this is a case where the register allocation is NOT the optimisation
//     level, which is the reflex MATCHED.md recommends and it is wrong here.
//
// The signed version at /O2 /Os is 2 of 9, so the level still matters once
// the type is right; /O2 is recorded.
//
// The rest of the reading, which was already right and is what made the
// residue legible as pure allocation:
//
// BOTH LOADS ARE HOISTED ABOVE BOTH STORES. Two constant offsets off one
// base provably cannot alias, so MSVC is free to software-pipeline the pair.
// Contrast src/n6_copy4_bytes.cpp, where two DIFFERENT pointers may alias and
// every load stays pinned behind the previous store.
//
// `li r10,1` + `slw r8,r10,r4` is `1 << n` with a variable shift, emitted
// ONCE for both updates. The or's operand order is not source-readable
// (MATCHED.md), so nothing is claimed from `or r5,r7,r8` versus `or r5,r8,r7`.
//
// Nothing is relocated; all 9 words are compared.

#include "types.h"

struct BitPair
{
    /* 0x00 */ s8 a;
    /* 0x01 */ s8 b;
};
ASSERT_OFFSET(BitPair, b, 0x01);

void SetBitBoth(BitPair* p, int n)
{
    int m = 1 << n;
    p->a |= m;
    p->b |= m;
}
