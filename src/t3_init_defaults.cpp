#include "types.h"

// sub_825E0118 -- initialise a large object to its defaults and return 0.
// 172 B, 7 callers, 12 float ops.
//
// Three pooled float constants, each with its own `lis`:
//      82002DA4 = 0.0f      82002D40 = 1.0f      82063470 = -666.0f
// and 666 / -1 as integer sentinels. The pair (666, -666.0f) is the tell that
// these are "unset" markers rather than real values.
//
// The emitted stores interleave one integer with one float almost throughout;
// per the two-stream rule each stream's INTERNAL order is source order and
// the interleaving is dual-issue scheduling. Both streams are in ascending
// address order here, so writing the whole thing in address order preserves
// both:
//      int    20 24 28 32 36 40 44(b) 48 52 56 60 64 68 72 76 80 88 92 140 144
//      float  84 96 100 104 108 112 116 120 124
//
// `mr r9,r3` up front and `li r3,0` in the middle: the object pointer has to
// vacate r3 because the function returns 0.

struct Defaults
{
    /* 0x00 */ char unk0000[0x14];
    /* 0x14 */ s32  f14;
    /* 0x18 */ s32  f18;
    /* 0x1C */ s32  f1C;
    /* 0x20 */ s32  f20;
    /* 0x24 */ s32  f24;
    /* 0x28 */ s32  f28;
    /* 0x2C */ u8   f2C;
    /* 0x2D */ char unk002D[0x03];
    /* 0x30 */ s32  f30;
    /* 0x34 */ s32  f34;
    /* 0x38 */ s32  f38;
    /* 0x3C */ s32  f3C;
    /* 0x40 */ s32  f40;
    /* 0x44 */ s32  f44;
    /* 0x48 */ s32  f48;
    /* 0x4C */ s32  f4C;
    /* 0x50 */ s32  f50;
    /* 0x54 */ f32  f54;
    /* 0x58 */ s32  f58;
    /* 0x5C */ s32  f5C;
    /* 0x60 */ f32  f60;
    /* 0x64 */ f32  f64;
    /* 0x68 */ f32  f68;
    /* 0x6C */ f32  f6C;
    /* 0x70 */ f32  f70;
    /* 0x74 */ f32  f74;
    /* 0x78 */ f32  f78;
    /* 0x7C */ f32  f7C;
    /* 0x80 */ char unk0080[0x0C];
    /* 0x8C */ s32  f8C;
    /* 0x90 */ s32  f90;
};
ASSERT_OFFSET(Defaults, f14, 0x14);
ASSERT_OFFSET(Defaults, f2C, 0x2C);
ASSERT_OFFSET(Defaults, f38, 0x38);
ASSERT_OFFSET(Defaults, f54, 0x54);
ASSERT_OFFSET(Defaults, f60, 0x60);
ASSERT_OFFSET(Defaults, f7C, 0x7C);
ASSERT_OFFSET(Defaults, f8C, 0x8C);
ASSERT_OFFSET(Defaults, f90, 0x90);

int InitDefaults(Defaults* d, s32 owner)
{
    d->f14 = owner;
    d->f18 = 0;
    d->f1C = 0;
    d->f20 = 0;
    d->f24 = 2;
    d->f28 = 0;
    d->f2C = 0;
    d->f30 = 0;
    d->f34 = 0;
    d->f38 = 666;
    d->f3C = -1;
    d->f40 = -1;
    d->f44 = -1;
    d->f48 = 0;
    d->f4C = -1;
    d->f50 = 0;
    d->f54 = 0.0f;
    d->f58 = 0;
    d->f5C = 0;
    d->f60 = 1.0f;
    d->f64 = 0.0f;
    d->f68 = 0.0f;
    d->f6C = 0.0f;
    d->f70 = 0.0f;
    d->f74 = -666.0f;
    d->f78 = -666.0f;
    d->f7C = -666.0f;
    d->f8C = 0;
    d->f90 = -1;
    return 0;
}
