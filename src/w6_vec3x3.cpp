#include "types.h"

// sub_82152300 -- copy three vec3s into a struct, then zero an int.
// 84 B, 3 callers.
//
// Loads f0/f13/f12 from r4, stores to r3+0/4/8; f11/f10/f9 from r5 to
// +16/20/24; f8/f7/f6 from r6 to +32/36/40; stw r11,12(r3) last.  Each
// vec3's three fields come out in address order; +12 sits between the
// groups and is written after the third group.

struct Vec3f
{
    float x, y, z;
};

struct ThreeVec
{
    /* 0x00 */ Vec3f a;
    /* 0x0C */ s32   f12;
    /* 0x10 */ Vec3f b;
    /* 0x20 */ Vec3f c;
};

void SetThree(ThreeVec* d, const Vec3f* a, const Vec3f* b, const Vec3f* c)
{
    d->a = *a;
    d->b = *b;
    d->c = *c;
    d->f12 = 0;
}

// NEAR-MISS. struct-assign spelled as three copies; image interleaves loads first.
