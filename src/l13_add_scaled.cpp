// sub_8214FDE8 -- accumulate a scaled vector and mark the object. 84 B,
// 4 callers.
//
//      lwz   r11,92(r3)
//      addi  r10,r11,152        DEAD -- r10 is next WRITTEN, never read
//      lfs   f0,136(r11) ; lfs f13,140(r11)
//      fmuls f12,f0,f1
//      lfs   f11,144(r11)
//      fmuls f10,f13,f1 ; fmuls f9,f11,f1
//      lfs   f8,152(r11) ; lfs f7,156(r11) ; lfs f6,160(r11)
//      fadds f5,f8,f12  ; stfs f5,152(r11)
//      fadds f4,f7,f10  ; stfs f4,156(r11)
//      fadds f3,f6,f9   ; stfs f3,160(r11)
//      lwz   r10,148(r11) ; ori r9,r10,2 ; stw r9,148(r11)
//
// THE DEAD `addi r10,r11,152` IS THE WHOLE SHAPE QUESTION.  An address
// computed and never read is what an inlined helper taking `&member` leaves
// behind (sub_82703E28, sub_82164040); a flat body folds every access into
// base+offset and never forms the pointer at all.  So the accumulate is
// written as a helper over the vector at 0x98 and the compiler re-folds its
// three accesses back onto r11, stranding the addi.
//
// THE SEPARATE fmuls AND fadds ARE A MEMORY ROUND-TRIP.  Under /fp:fast
// MSVC contracts `d = d + s * k` into one `fmadds` -- measured, both as
// three `+=` statements and as three named product locals, which is 1 of 17.
// Holding the three products in a LOCAL ARRAY instead separates them: the
// multiply writes memory and the add reads it, so instruction selection sees
// two trees and never matches the fmadd pattern, and the store/load pair is
// forwarded away afterwards.  That is the same lever recorded for constant
// folding -- a memory round-trip survives where a local does not.
//
// /fp:precise also un-contracts them, but it is the wrong answer here: it
// reorders the whole body (8 of 21) and nothing else in this function could
// distinguish the model, which is exactly the trap MATCHED.md records.
//
// All three products are computed before any destination component is
// loaded, which needs no lever here: both vectors hang off ONE pointer at
// constant offsets, so MSVC can prove the stores do not alias the loads.
//
// The flag word at 0x94 sits BETWEEN the two vectors and is read back after
// the three stores -- a different offset off the same base, so that reload is
// ordinary, not aliasing.

#include "types.h"

struct Vec3
{
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
};

struct Body
{
    /* 0x0000 */ char unk0000[0x88];
    /* 0x0088 */ Vec3 delta;
    /* 0x0094 */ u32  flags;
    /* 0x0098 */ Vec3 total;
};
ASSERT_OFFSET(Body, delta, 0x88);
ASSERT_OFFSET(Body, flags, 0x94);
ASSERT_OFFSET(Body, total, 0x98);

struct BodyOwner
{
    /* 0x00 */ char  unk0000[0x5C];
    /* 0x5C */ Body* body;
};
ASSERT_OFFSET(BodyOwner, body, 0x5C);

static void AddScaled(Vec3* d, const Vec3* s, float k)
{
    float p[3];

    p[0] = s->x * k;
    p[1] = s->y * k;
    p[2] = s->z * k;

    d->x = d->x + p[0];
    d->y = d->y + p[1];
    d->z = d->z + p[2];
}

void Accumulate(BodyOwner* o, float k)
{
    Body* b = o->body;

    AddScaled(&b->total, &b->delta, k);
    b->flags |= 2;
}
