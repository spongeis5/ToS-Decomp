#include "types.h"

// sub_82154ED8 -- quaternion to 3x3 rotation matrix, 16-byte row stride.
// 176 B, 12 callers.  r3 = quaternion (x y z w at 0 4 8 12), r4 = matrix.
//
//   m00 = 1 - yy - zz    m01 = xy + wz    m02 = xz - wy
//   m10 = xy - wz        m11 = 1 - zz - xx  m12 = yz + wx
//   m20 = xz + wy        m21 = yz - wx    m22 = 1 - xx - yy
//
// with x2 = 2x, xx = x*x2, xy = x*y2 and so on. The diagonal is CYCLIC in the
// order it subtracts -- (yy,zz), (zz,xx), (xx,yy) -- and each is two separate
// fsubs off the pooled 1.0f, so the source is `1.0f - yy - zz` and not
// `1.0f - (yy + zz)`. The subtractions are not commutative, so that IS
// readable, unlike the operand order of the products.
//
// `stw r9,12(r4)` writes an INTEGER zero at 0x0C. 0.0f would have come out of
// the pool as an stfs, as it does in sub_82799B98, so that field is not a
// float. Offsets 0x1C and 0x2C are never written at all.
//
// NOT A MATCH: 25 of 44 words at /O2, 176 bytes -- the size, the instruction
// multiset and every load and store ADDRESS are right. What is left is three
// registers, and everything after them is downstream of those three.
//
// TWO THINGS HERE ARE READABLE, AND BOTH WERE MEASURED THE HARD WAY.
//
// 1. THE INTEGER ZERO STORE GOES LAST. Written first -- which is where the
//    target EMITS it, at 82154EF4, before any float arithmetic -- the whole
//    function is 2 of 44 and even the four component loads land in the wrong
//    registers. Written last it is 6 of 44 immediately, `li r9,0`,
//    `lfs f9,12(r3)` and `stw r9,12(r4)` all agree, and the mapping
//    f12=x f11=y f10=z f9=w is the target's. This is a counter-example to
//    "store order is source order" that costs the whole function: a store
//    with no float dependency is hoisted past thirteen of them, so its
//    emitted position says nothing about where it was written.
//
// 2. THE SIX CROSS PRODUCTS EMIT IN SOURCE ORDER, and the target's is
//    INTERLEAVED rather than grouped: wz, xy, wy, wx, xz, yz. Not
//    xy,xz,yz,wx,wy,wz and not wx,wy,wz,xy,xz,yz. Reading that order off the
//    target took the function from 10 to 24 of 44 in one edit; the near
//    neighbour wz,wy,xy,wx,xz,yz is 25.
//
// WHAT IS LEFT, precisely:
//
//     82154F18  want fmuls f0,f9,f8    (wz -> f0)    got f2
//     82154F1C  want fmuls f10,f12,f7  (xy -> f10)   got wx
//     82154F24  want fmuls f2,f9,f6    (wx -> f2)    got xy -> f0
//
// f0 held 2.0f and is dead after x2. The target recycles it for `wz`; this
// source recycles it for `xy`. Every later difference is that swap carried
// forward -- the six add/sub pairs and the three diagonal terms all compute
// the right value from the right inputs into the wrong register.
//
// WHAT DOES NOT REACH IT:
//
//   * all 720 permutations of the six cross-product declarations -- 25 is the
//     ceiling, and 24 of the 720 reach 20 or better;
//   * all 216 combinations of the doubling and squaring declaration orders;
//   * all 24 orders of four component locals, and all 8 array/scalar
//     combinations of the doubled components, the squares and the products --
//     the sub_82691C50 lever does not move this one;
//   * twelve shapes: direct member reads, scalar locals, `float s[4]`, a
//     const reference parameter, the quaternion as `const float*`, the matrix
//     as `float*`, products written inline, `2.0f * c`;
//   * six member-function forms, which is what decided sub_826919A8 -- the
//     best is 6 of 44;
//   * all 72 flag combinations tools/flagsweep.py builds, including /Ou
//     (prescheduling). Every one is 176 bytes; /O2 is 25, /O2 /Os and /O1 are
//     8. So the level is /O2 and the flag explanation is exhausted.

// EXHAUSTIVE ON THE ONE AXIS THAT LOOKED LIKE THE ANSWER. The two wrong
// `fmuls` at 82154F1C and 82154F24 compute the same six cross products in a
// different ORDER -- the target's schedule is wz, xy, wy, wx, xz, yz and this
// source's comes out wz, wx, wy, xy, xz, yz -- so the declaration order of
// the six products was the obvious lever. ALL 720 PERMUTATIONS were compiled
// at /O2. Every one is 176 bytes, the correct size, and the best score over
// all 720 is 25 of 40 -- the order already in this file. Sixteen orderings
// tie at 24, and the rest are worse; none reaches 26.
//
// So the schedule of the six products is not chosen by their source order,
// and the fifteen remaining words are float REGISTER assignment downstream of
// that choice. /O2 /Os is 8 of 40 for every one of the 720, so the level is
// settled as plain /O2.
//
// THE ARRAY-STAGING LEVER WAS TRIED AND DOES NOT REACH IT. That is the shape
// that took sub_82691C50 from 31 of 39 to 39 of 39, and it is the natural
// next move when the instructions are right and the float registers are not.
// Four stagings are all 176 bytes and none beats the scalars:
//
//     six scalars (this file)          25 of 40
//     `float c[6]` + `float d[3]`      25 of 40
//     one `float p[9]` for everything  25 of 40
//     the three `1.0f -` terms named   25 of 40
//     the quaternion snapshotted first 19 of 40   (worse)
//
// Byte-identical output for the first four, so MSVC flattens the arrays back
// to the same value numbers here -- unlike sub_82691C50, where the array was
// a SNAPSHOT of memory that could otherwise alias the destination. Here the
// products are already pure arithmetic on values in registers, so there is
// nothing for the staging to pin.
//
// AND WHICH PRODUCTS ARE NAMED IS SETTLED TOO: ALL SIX. Every shape recorded
// above declares all six as locals or writes all six inline, so the 64
// SUBSETS were the axis left open -- a named local is created where it is
// declared and an inline expression at its use, which is the control that
// finished sub_82600960 and is what the float register assignment here
// follows. All 64 were compiled. Naming all six is 25 of 40 at 176 bytes;
// the best of the other 63 is 6 of 40, and most are 2 to 5. There is no
// partial naming that helps, and the ones that drop a product go to 180
// bytes. That closes the axis rather than leaving it as an untried idea.
struct Quat { f32 x; f32 y; f32 z; f32 w; };
ASSERT_OFFSET(Quat, z, 0x08);
ASSERT_OFFSET(Quat, w, 0x0C);

struct Mtx33
{
    f32 m00; f32 m01; f32 m02; s32 a0C;
    f32 m10; f32 m11; f32 m12; f32 a1C;
    f32 m20; f32 m21; f32 m22;
};
ASSERT_OFFSET(Mtx33, a0C, 0x0C);
ASSERT_OFFSET(Mtx33, m10, 0x10);
ASSERT_OFFSET(Mtx33, m12, 0x18);
ASSERT_OFFSET(Mtx33, m20, 0x20);
ASSERT_OFFSET(Mtx33, m22, 0x28);

void QuatToMtx(const Quat* q, Mtx33* m)
{
    float x2 = q->x * 2.0f;
    float y2 = q->y * 2.0f;
    float z2 = q->z * 2.0f;

    float xx = q->x * x2;
    float yy = q->y * y2;
    float zz = q->z * z2;

    float wz = q->w * z2;
    float wy = q->w * y2;
    float xy = q->x * y2;
    float wx = q->w * x2;
    float xz = q->x * z2;
    float yz = q->y * z2;

    m->m01 = xy + wz;
    m->m10 = xy - wz;
    m->m02 = xz - wy;
    m->m20 = xz + wy;
    m->m12 = yz + wx;
    m->m21 = yz - wx;
    m->m00 = 1.0f - yy - zz;
    m->m11 = 1.0f - zz - xx;
    m->m22 = 1.0f - xx - yy;

    m->a0C = 0;
}
