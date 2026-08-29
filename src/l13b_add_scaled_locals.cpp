// sub_8214FDE8 -- variant B: the three products named as locals first.
// Probe for why the target has separate fmuls/fadds where /fp:fast contracts
// into fmadds.  Not a claim; see l13_add_scaled.cpp for the reading.

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

void Accumulate(BodyOwner* o, float k)
{
    Body* b = o->body;

    float px = b->delta.x * k;
    float py = b->delta.y * k;
    float pz = b->delta.z * k;

    b->total.x += px;
    b->total.y += py;
    b->total.z += pz;

    b->flags |= 2;
}
