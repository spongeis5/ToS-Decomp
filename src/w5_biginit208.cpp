#include "types.h"

// sub_82763808 -- a wide initialiser: integers first, then floats.
// 208 B, 4 callers.
//
// Integer stream (emitted order, then scheduled against the float stream
// by dual-issue): zeros at 0,4,8,12,20,24; -1 at 28; zeros 32..56; 72,76;
// h80; 84,88; -1 at 92; 96..108; h112; 116..144; 148,152; b160,b161; 164.
// Float stream: 0.0f at 16, 60, 64, 68 (one lfs of 82002DA4); 1.0f at 156
// (82002D40).
//
// Per MATCHED.md's two-streams lever, the source writes the integer stores
// in their emitted relative order and THEN the float stores in theirs --
// reading the merged emitted order back as source order is actively worse.

struct Big208
{
    /* 0x00 */ s32   f0;
    /* 0x04 */ s32   f4;
    /* 0x08 */ s32   f8;
    /* 0x0C */ s32   f12;
    /* 0x10 */ float f16;
    /* 0x14 */ s32   f20;
    /* 0x18 */ s32   f24;
    /* 0x1C */ s32   f28;
    /* 0x20 */ s32   f32;
    /* 0x24 */ s32   f36;
    /* 0x28 */ s32   f40;
    /* 0x2C */ s32   f44;
    /* 0x30 */ s32   f48;
    /* 0x34 */ s32   f52;
    /* 0x38 */ s32   f56;
    /* 0x3C */ float f60;
    /* 0x40 */ float f64;
    /* 0x44 */ float f68;
    /* 0x48 */ s32   f72;
    /* 0x4C */ s32   f76;
    /* 0x50 */ u16   f80;
    /* 0x52 */ char  unk0052[2];
    /* 0x54 */ s32   f84;
    /* 0x58 */ s32   f88;
    /* 0x5C */ s32   f92;
    /* 0x60 */ s32   f96;
    /* 0x64 */ s32   f100;
    /* 0x68 */ s32   f104;
    /* 0x6C */ s32   f108;
    /* 0x70 */ u16   f112;
    /* 0x72 */ char  unk0072[2];
    /* 0x74 */ s32   f116;
    /* 0x78 */ s32   f120;
    /* 0x7C */ s32   f124;
    /* 0x80 */ s32   f128;
    /* 0x84 */ s32   f132;
    /* 0x88 */ s32   f136;
    /* 0x8C */ s32   f140;
    /* 0x90 */ s32   f144;
    /* 0x94 */ s32   f148;
    /* 0x98 */ s32   f152;
    /* 0x9C */ float f156;
    /* 0xA0 */ u8    f160;
    /* 0xA1 */ u8    f161;
    /* 0xA2 */ char  unk00A2[2];
    /* 0xA4 */ s32   f164;
};

void InitBig208(Big208* b)
{
    const s32 z = 0;
    const s32 u = -1;
    b->f0   = z;
    b->f4 = z;
    b->f8 = z;
    b->f12 = z;
    b->f20 = z;
    b->f24 = z;
    b->f28 = u;
    b->f32 = z;
    b->f36 = z;
    b->f40 = z;
    b->f44 = z;
    b->f48 = z;
    b->f52 = z;
    b->f56 = z;
    b->f72 = z;
    b->f76 = z;
    b->f80  = 0;
    b->f84 = z;
    b->f88 = z;
    b->f92 = u;
    b->f96 = z;
    b->f100 = 0;
    b->f104 = 0;
    b->f108 = 0;
    b->f112 = 0;
    b->f116 = 0;
    b->f120 = 0;
    b->f124 = 0;
    b->f128 = 0;
    b->f132 = 0;
    b->f136 = 0;
    b->f140 = 0;
    b->f144 = 0;
    b->f148 = 0;
    b->f152 = 0;
    b->f160 = 0;
    b->f161 = 0;
    b->f164 = 0;

    b->f16  = 0.0f;
    b->f60  = 0.0f;
    b->f64  = 0.0f;
    b->f68  = 0.0f;
    b->f156 = 1.0f;
}

// NEAR-MISS (37 words). The target materialises BOTH -1 constants up front
// (li r9,-1 / li r8,-1 in the first five instructions) and assigns every
// store register from one early allocation; ours creates -1 lazily at the
// f28 store. Named-constant spelling (const s32 u = -1) did not reproduce
// the hoist. Same register-assignment wall as the other near-misses; the
// store ORDER and every offset agree.
