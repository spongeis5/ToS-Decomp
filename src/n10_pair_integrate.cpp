// sub_8277DFF0 -- integrate four (rate, value) pairs and then decay the four
// rates. 132 B, 4 callers, 32 float ops. /O2 /Os.
//
//      lfs f0,4(r4) ; lfs f13,0(r3) ; lfs f12,4(r3)
//      fmadds f0,f0,f13,f12  ; stfs f0,4(r3)      d0 += b0 * v0
//      lfs f0,8(r3) ; lfs f12,12(r3) ; lfs f11,12(r4)
//      fmadds f12,f11,f0,f12 ; stfs f12,12(r3)    d1 += b1 * v1
//      lfs f11,16(r3) ; lfs f12,20(r3) ; lfs f10,20(r4)
//      fmadds f12,f10,f11,f12 ; lfs f10,24(r3) ; stfs f12,20(r3)
//      lfs f9,28(r3) ; lfs f12,28(r4)
//      fmadds f12,f12,f10,f9 ; stfs f12,28(r3)    d3 += b3 * v3
//      lfs f12,0(r4)  ; fmuls f13,f12,f13 ; stfs f13,0(r3)   v0 *= a0
//      lfs f13,8(r4)  ; fmuls f0,f13,f0   ; stfs f0,8(r3)    v1 *= a1
//      lfs f0,16(r4)  ; fmuls f0,f0,f11   ; stfs f0,16(r3)   v2 *= a2
//      lfs f0,24(r4)  ; fmuls f0,f0,f10   ; stfs f0,24(r3)   v3 *= a3
//
// TWO SEPARATE THINGS HAD TO BE RIGHT, and each was diagnosed on its own.
//
// (1) THE LEVEL IS /O2 /Os, and the signature is the one MATCHED.md calls the
// single most useful thing to know: `fmadds f0,f0,f13,f12` REUSES its own
// input f0 as the destination, where /O2 gives it a fresh f11 and then
// renames every float in the function -- 5 of 33 with identical instructions
// in identical order. Changing the flag before touching a line of C took it
// to 32 of 33.
//
// (2) THE FOUR `v` VALUES ARE NAMED IN LOCALS. The one remaining word was
// the first `fmuls`: the image has `fmuls f13,f12,f13` -- b's freshly loaded
// value in the A slot, which is what the other three multiplies also do --
// and every spelling that reads `a[0].v` again in the second group puts a's
// long-lived value there instead, `fmuls f13,f13,f12`. Ten shapes are all
// 32 of 33: `*=`, `a = b * a`, `a = a * b`, those two mixed for the first
// term only, the `d` group as an explicit sum, the `d` group with the
// multiply written first, a flat eight-field struct in both multiply orders,
// and a non-const second parameter. Hoisting the four `v` reads into locals
// is 33 of 33.
//
// That is consistent with MATCHED.md's negative result rather than against
// it: the A-slot operand of a commutative float op is NOT decided by the
// source's written order -- all ten of those spellings prove it, since they
// include both orders and change nothing -- it is decided by WHICH READ IS
// THE CSE REPRESENTATIVE, and a named local is what moves that.
//
// THE TWO GROUPS ARE THE TWO HALVES OF THE SOURCE, not one interleaved loop
// body. f13, f0, f11 and f10 hold the four `v` values across four intervening
// stores and are never reloaded; a loop with both statements in it, unrolled,
// would put each pair's two updates together. Written as two loops it is
// 80 bytes -- MSVC leaves them rolled -- so the unrolling is in the source.
//
// Everything the compiler does move is downward. A load emitted after a store
// is legal whichever way the pointers alias, so `b`'s four `v` fields being
// read at the very end says nothing about aliasing; the `v` values staying
// live across the stores is the part that carries information, and it needs
// only the two constant offsets within ONE object to be distinct.
//
// The pairs sit at (0,4), (8,12), (16,20), (24,28): a 4-element array of an
// 8-byte pair, or a struct of four of them -- the bytes do not distinguish
// those (the flat-struct spelling scores the same 32 of 33), and the array
// form is written because both operands are indexed the same way.
//
// Nothing is relocated; all 33 words are compared.

#include "types.h"

struct RatePair
{
    /* 0x00 */ f32 v;
    /* 0x04 */ f32 d;
};
ASSERT_SIZE(RatePair, 8);

void IntegratePairs(RatePair* a, const RatePair* b)
{
    f32 v0 = a[0].v;
    f32 v1 = a[1].v;
    f32 v2 = a[2].v;
    f32 v3 = a[3].v;

    a[0].d += b[0].d * v0;
    a[1].d += b[1].d * v1;
    a[2].d += b[2].d * v2;
    a[3].d += b[3].d * v3;

    a[0].v = b[0].v * v0;
    a[1].v = b[1].v * v1;
    a[2].v = b[2].v * v2;
    a[3].v = b[3].v * v3;
}
