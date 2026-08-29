#include "types.h"

// sub_8214F7E8 -- project a vector onto each of a matrix's three basis rows,
// dividing each dot product by that row's squared length. 188 B, 17 callers.
// r3 = out, r4 = matrix (rows at 0, 16, 32 -- a 16-byte row stride),
// r5 = the vector.
//
// NOT A MATCH, and not close: this source is 204 bytes and 1 of 47 words.
// It is kept because the SHAPE below was measured off the image and is worth
// having written down before anyone else opens this one.
//
//   out->x = dot(row0,v) / lenSq(row0)
//   out->y = dot(row1,v) / lenSq(row1)
//   out->z = dot(row2,v) / lenSq(row2)
//
// Six three-term sums, each one `fmuls` + two `fmadds`: 6 fmuls, 12 fmadds,
// 3 fdivs. Those 21 words are not in doubt -- the arithmetic reads cleanly.
// What has to be reproduced is everything around them.
//
// THREE DEAD INSTRUCTIONS AND ONE DEAD STORE:
//
//   8214F7EC  addi r11,r4,16     r11 never read
//   8214F7FC  addi r11,r4,32     r11 never read, and overwrites the first
//   8214F878  stfs f4,0(r3)      overwritten at 8214F888 with no load between
//   8214F89C  fmr  f3,f4         f3 never read
//
// The two `addi`s are `&m->r1` and `&m->r2`, and there is NO `addi r11,r4,0`
// for row 0 because that address is already r4. That is the signature of an
// inline helper taking a pointer or reference to a sub-object: MSVC
// materialised the argument, folded every load through it into a displacement
// off r4, and left the address computation behind.
//
// THE LOAD CENSUS SAYS THE SAME THING. 18 loads: 3 from the vector, 15 from
// the matrix. Split by row:
//
//   row 0 (offsets 0, 4, 8)      3 loads   each component loaded ONCE
//   row 1 (offsets 16, 20, 24)   6 loads   each component loaded TWICE
//   row 2 (offsets 32, 36, 40)   6 loads   each component loaded TWICE
//
// So row 0 is shared between its dot product and its squared length and rows
// 1 and 2 are not -- the same CSE asymmetry the dead `addi`s point at, and
// the same lever that decided sub_821FF908 in this batch (an expression
// spelled two ways is not CSEd). Any candidate that loads all three rows the
// same number of times is the wrong shape regardless of its score.
//
// Six shapes were tried, all 0 or 1 of 47: inline helpers taking `const Row*`
// and taking `const Row&`; the divide written as a second statement on
// `out->x` (which is what the dead store at 8214F878 looks like); all three
// dot products stored before any of the three divides; and the whole thing
// written out as three inline expressions with no helpers, which is 172 bytes
// because it CSEs all three rows.
//
// THE SIZE IS NOW EXACTLY RIGHT: 188 bytes, down from 224, by naming ALL SIX
// intermediates -- three dot products and three squared lengths -- as locals
// before any store. That is MATCHED.md's sub_82154A68 lever ("naming a
// sub-expression as a local lets the values be computed up front"), and it
// is what produces the target's fully software-pipelined block of eighteen
// loads and eighteen multiplies ahead of the four stores. Naming only the
// three dot products is 196 bytes; naming none is 224.
//
// The word count does not follow the size: still 1 of 47 at /O2 and 2 at
// /Os. Every load, every `fmuls`/`fmadds` and every `fdivs` is present and
// the schedule interleaves them the way the target does, but the float
// register assignment is different throughout, which is the same class of
// stall as sub_82691C50 before `float s[6]` cracked it.
//
// EIGHT SHAPES MEASURED IN THIS ROUND, with their sizes, so the size axis is
// on the record even where the words are not:
//
//     dots and lens named first        188 B   1 of 47   <- this
//     the whole thing returned BY VALUE 188 B  1 of 47
//     three dots named first           196 B   1 of 47
//     two-level Axis() helper          212 B   0 of 47
//     `f32* px = &out->x` pinned       224 B   1 of 47
//     pin + the row-0 dot in a local   220 B   1 of 47
//     helpers taking `const Row&`      224 B   1 of 47
//     the baseline                     224 B   1 of 47
//
// THE BY-VALUE RETURN IS ALSO 188 BYTES, which matters because that
// signature -- r3 as the hidden return buffer rather than an out parameter
// -- is what solved sub_821A5270 in this same batch. Here the two are
// indistinguishable by size and by score, so this file keeps the out-pointer
// form and records that the other is equally close.
//
// THE ADDRESS-OF PIN DOES NOT EXPLAIN THE DEAD STORE. `f32* px = &out->x`
// was the obvious reading of `stfs f4,0(r3)` surviving nine instructions
// before being overwritten -- a pinned store is exactly what dead-store
// elimination cannot remove -- and it costs 36 bytes rather than buying
// anything. So the dead store has another cause, and the load census (row 0
// loaded three times, rows 1 and 2 six times each) is still the strongest
// unexplained fact about this function.

struct V3  { f32 x; f32 y; f32 z; };
struct Row { f32 x; f32 y; f32 z; f32 w; };
struct Mtx43 { Row r0; Row r1; Row r2; };
ASSERT_OFFSET(Mtx43, r1, 0x10);
ASSERT_OFFSET(Mtx43, r2, 0x20);

static float Dot3(const Row* a, const V3* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static float LenSq3(const Row* a)
{
    return a->x * a->x + a->y * a->y + a->z * a->z;
}

void ProjectOntoAxes(V3* out, const Mtx43* m, const V3* v)
{
    f32 d0 = Dot3(&m->r0, v);
    f32 l0 = LenSq3(&m->r0);
    f32 d1 = Dot3(&m->r1, v);
    f32 l1 = LenSq3(&m->r1);
    f32 d2 = Dot3(&m->r2, v);
    f32 l2 = LenSq3(&m->r2);

    out->x = d0;
    out->x = d0 / l0;
    out->y = d1 / l1;
    out->z = d2 / l2;
}
