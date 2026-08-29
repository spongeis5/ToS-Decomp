// sub_825A36C0 -- clamp a count to at least one, remember it, and derive an
// integer rate from it unless the scaled period is too small.
// 128 B, 5 callers, 14 float ops.
//
//   mr r11,r3 ; cmpwi cr6,r4,1 ; bge- ; li r4,1
//
// `this` is moved out of r3 because r3 carries a RETURN VALUE -- `li r3,0` at
// 825A36FC, on the only path that reaches either exit, so the function is an
// `int` that always returns zero rather than `void`.
//
//   extsw / std -16(r1) / lfd / fcfid / frsp
//
// twice: MSVC's `int` to `float` conversion on this target. The `extsw` says
// both converted values are signed.
//
// The two constants are read out of .rdata and the words are relocated:
//   82002FC4 = 3ECCCCCD = 0.4f      (the multiplier)
//   82067C40 = 3C23D70A = 0.01f     (the floor)
//
//   fmuls f8,f9,f0 ; fmuls f0,f8,f12
//
// multiplies the converted count by 0.4f FIRST and the object's scale second.
// The operand slots of a commutative float multiply carry no information
// under /fp:fast (MATCHED.md), so nothing is claimed about the written order
// within either product -- but the ASSOCIATION does, and it cost two words:
//
//   LEVER: /fp:fast REASSOCIATES `a * C * b`, and explicit PARENTHESES stop
//   it. Written `(float)n * 0.4f * r->scale`, which already associates left
//   in C, MSVC sinks the constant and emits `(scale * n) * 0.4f` --
//   `fmuls f8,f12,f9 ; fmuls f0,f8,f0`, 26 of 28. Written
//   `((float)n * 0.4f) * r->scale` it is 28 of 28. Thirteen shapes were
//   measured: every one that names the first product -- a temp, a `*=`, a
//   `float fn` local plus a temp, an inlined helper returning it -- is also
//   28 of 28, and every one that leaves the three factors in a single
//   unparenthesised chain is 26 of 28, whichever order they are written in
//   (`0.4f * n * scale` and `fn * 0.4f * scale` both fail the same way).
//   So the tell is whether the first product is a SEPARATE EXPRESSION, not
//   which operand comes first.
//
//   fcmpu cr6,f0,f13 ; bltlr cr6
//
// is a guard written as a conditional RETURN, with the interesting path as
// the fall-through, so it is spelled `if (x < 0.01f) return 0;`.
//
// `fctiwz` + `stfiwx` is a float truncated into an `int` field, which pins
// 1252 as `int` rather than `float`.

#include "types.h"

struct Source
{
    /* 0x000 */ u8  unk0000[0x108];
    /* 0x108 */ int total;
};

ASSERT_OFFSET(Source, total, 0x108);

struct Rate
{
    /* 0x000 */ u8      unk0000[0x20];
    /* 0x020 */ Source* src;
    /* 0x024 */ u8      unk0024[0x4C0];
    /* 0x4E4 */ int     step;
    /* 0x4E8 */ u8      unk04E8[0x374];
    /* 0x85C */ f32     scale;
    /* 0x860 */ u8      unk0860[0x1C];
    /* 0x87C */ int     count;
};

ASSERT_OFFSET(Rate, src, 0x20);
ASSERT_OFFSET(Rate, step, 0x4E4);
ASSERT_OFFSET(Rate, scale, 0x85C);
ASSERT_OFFSET(Rate, count, 0x87C);

int RateFromScale(Rate* r, int n)
{
    if (n < 1)
        n = 1;

    r->count = n;

    float period = ((float)n * 0.4f) * r->scale;

    if (period < 0.01f)
        return 0;

    r->step = (int)((float)r->src->total / period);
    return 0;
}
