// sub_8214FDE8 -- variant C: the products held in a local float[3].
// Probe only; see l13_add_scaled.cpp for the reading.

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
    float p[3];

    p[0] = b->delta.x * k;
    p[1] = b->delta.y * k;
    p[2] = b->delta.z * k;

    b->total.x = b->total.x + p[0];
    b->total.y = b->total.y + p[1];
    b->total.z = b->total.z + p[2];

    b->flags |= 2;
}
