#include "types.h"

// sub_82154A68 -- the translation row of a 4x4 matrix product, then a tail
// call to the function that does the upper 3x3. 124 B, 34 callers.
// r3 = out, r4 = a, r5 = b.
//
//   out.m3j = a.m30*b.m0j + a.m31*b.m1j + a.m32*b.m2j + b.m3j   for j = 0,1,2
//   b 0x82154738
//
// The row stride is 16 bytes -- rows at 0, 16, 32, 48 -- and the same layout
// reads correctly in sub_82154738, the 452-byte function this tail-calls,
// which computes rows 0..2 of the same product. So the two together are one
// 4x4 multiply, split with the translation row peeled off in front.
//
// THE SHAPE THAT MATTERS: the three dot products are computed BEFORE any
// store, fully interleaved, and only the `+ b->m3j` addends are reloaded
// afterwards -- b->m31 after the store to out->m30, b->m32 after the store to
// out->m31.
//
// Writing the three assignments directly,
//
//     out->m30 = a->m30*b->m00 + ... + b->m30;
//     out->m31 = ...;
//
// compiles to 148 bytes, not 124, and to 0 of 31 words: each store may alias
// `b`, so MSVC finishes and stores one row element before it will touch `b`
// again, and reloads a->m30/m31/m32 for every element. Naming the three dot
// products as locals first is what lets them all be computed up front, and
// the surviving reloads of b->m31 and b->m32 are exactly the aliasing the
// compiler could NOT get rid of. 0 of 31 to 30 of 30 on that one change.
//
// Operand order within each product does not matter, nor does which side of
// the final `+` the translation is written on: all four orderings give the
// same 30 words.

struct Mtx4
{
    f32 m00, m01, m02, m03;
    f32 m10, m11, m12, m13;
    f32 m20, m21, m22, m23;
    f32 m30, m31, m32, m33;
};
ASSERT_OFFSET(Mtx4, m10, 0x10);
ASSERT_OFFSET(Mtx4, m20, 0x20);
ASSERT_OFFSET(Mtx4, m30, 0x30);

void MulUpper3x3(Mtx4* out, const Mtx4* a, const Mtx4* b);

void MulTranslationRow(Mtx4* out, const Mtx4* a, const Mtx4* b)
{
    float x = a->m30 * b->m00 + a->m31 * b->m10 + a->m32 * b->m20;
    float y = a->m30 * b->m01 + a->m31 * b->m11 + a->m32 * b->m21;
    float z = a->m30 * b->m02 + a->m31 * b->m12 + a->m32 * b->m22;

    out->m30 = x + b->m30;
    out->m31 = y + b->m31;
    out->m32 = z + b->m32;

    MulUpper3x3(out, a, b);
}
