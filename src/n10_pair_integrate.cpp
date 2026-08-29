// sub_8277DFF0 -- integrate four (rate, value) pairs and then decay the four
// rates. 132 B, 4 callers, 32 float ops.
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
// THE FOUR `v` VALUES ARE NEVER RELOADED. f13, f0, f11 and f10 are loaded
// once each in the first group and are still live when the second group
// multiplies them, across four intervening stores -- so the two groups are
// the two halves of the SOURCE, not one interleaved loop body. A loop with
// both statements in it, unrolled, would put each pair's two updates
// together; this puts all four of one kind and then all four of the other.
//
// Everything the compiler does move is downward. A load emitted after a store
// is always legal whichever way the pointers alias, so `b`'s four `a` fields
// being read at the very end says nothing about aliasing and nothing about
// where they were written; the `v` values staying live across the stores is
// the part that does carry information, and it only needs the two constant
// offsets within ONE object to be distinct.
//
// Operand order inside `fmadds` and `fmuls` is NOT source-readable under
// /fp:fast (MATCHED.md, measured on three functions and all 16 flip
// combinations of one), so nothing is claimed from f0*f13 versus f13*f0.
//
// The pairs sit at (0,4), (8,12), (16,20), (24,28): a 4-element array of an
// 8-byte pair, or a struct of four of them -- the bytes do not distinguish
// those and the array form is written because both operands are indexed the
// same way.
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
    a[0].d += b[0].d * a[0].v;
    a[1].d += b[1].d * a[1].v;
    a[2].d += b[2].d * a[2].v;
    a[3].d += b[3].d * a[3].v;

    a[0].v *= b[0].v;
    a[1].v *= b[1].v;
    a[2].v *= b[2].v;
    a[3].v *= b[3].v;
}
