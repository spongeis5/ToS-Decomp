#include "types.h"

// sub_8289FA50 -- clear a mixer-shaped block. 268 B.
//
// Chosen for STRUCTURE: 63 distinct fields written, the most of any
// unmatched function in the image, and every one of them pinned by a single
// match. 52 floats at 0x00..0xCC and 11 bytes at 0xD0..0xDA, so sizeof is at
// least 0xDB and the boundary between the two runs is exact.
//
// 52 floats and 11 flags, all zeroed, with no vtable and no pointer: this is
// a table of per-channel levels beside a table of per-channel booleans. 52 is
// not a round number, which is itself informative -- it is the channel count.
//
// THE TWO STREAMS ARE THE WHOLE OF IT. The emitted code alternates
// byte, float, byte, float all the way down, which reads as a source that
// alternates and is not: it is dual-issue scheduling of two independent
// store streams. Written as all eleven bytes in their emitted order and then
// all fifty-two floats in theirs -- the rule MATCHED.md records from
// sub_8214CCB8 -- the alternation comes out on its own.
//
// This file is GENERATED-looking on purpose. Sixty-three hand-typed
// assignments is exactly the kind of thing a transcription slip hides in, so
// the field names carry their own offsets: gain00 is at 0x00, flagD5 at
// 0xD5. A wrong one is visible without counting, and ASSERT_OFFSET catches
// it at compile time regardless.
struct MixerLevels
{
    /* 0x00 */ f32 gain00;
    /* 0x04 */ f32 gain04;
    /* 0x08 */ f32 gain08;
    /* 0x0C */ f32 gain0C;
    /* 0x10 */ f32 gain10;
    /* 0x14 */ f32 gain14;
    /* 0x18 */ f32 gain18;
    /* 0x1C */ f32 gain1C;
    /* 0x20 */ f32 gain20;
    /* 0x24 */ f32 gain24;
    /* 0x28 */ f32 gain28;
    /* 0x2C */ f32 gain2C;
    /* 0x30 */ f32 gain30;
    /* 0x34 */ f32 gain34;
    /* 0x38 */ f32 gain38;
    /* 0x3C */ f32 gain3C;
    /* 0x40 */ f32 gain40;
    /* 0x44 */ f32 gain44;
    /* 0x48 */ f32 gain48;
    /* 0x4C */ f32 gain4C;
    /* 0x50 */ f32 gain50;
    /* 0x54 */ f32 gain54;
    /* 0x58 */ f32 gain58;
    /* 0x5C */ f32 gain5C;
    /* 0x60 */ f32 gain60;
    /* 0x64 */ f32 gain64;
    /* 0x68 */ f32 gain68;
    /* 0x6C */ f32 gain6C;
    /* 0x70 */ f32 gain70;
    /* 0x74 */ f32 gain74;
    /* 0x78 */ f32 gain78;
    /* 0x7C */ f32 gain7C;
    /* 0x80 */ f32 gain80;
    /* 0x84 */ f32 gain84;
    /* 0x88 */ f32 gain88;
    /* 0x8C */ f32 gain8C;
    /* 0x90 */ f32 gain90;
    /* 0x94 */ f32 gain94;
    /* 0x98 */ f32 gain98;
    /* 0x9C */ f32 gain9C;
    /* 0xA0 */ f32 gainA0;
    /* 0xA4 */ f32 gainA4;
    /* 0xA8 */ f32 gainA8;
    /* 0xAC */ f32 gainAC;
    /* 0xB0 */ f32 gainB0;
    /* 0xB4 */ f32 gainB4;
    /* 0xB8 */ f32 gainB8;
    /* 0xBC */ f32 gainBC;
    /* 0xC0 */ f32 gainC0;
    /* 0xC4 */ f32 gainC4;
    /* 0xC8 */ f32 gainC8;
    /* 0xCC */ f32 gainCC;
    /* 0xD0 */ u8  flagD0;
    /* 0xD1 */ u8  flagD1;
    /* 0xD2 */ u8  flagD2;
    /* 0xD3 */ u8  flagD3;
    /* 0xD4 */ u8  flagD4;
    /* 0xD5 */ u8  flagD5;
    /* 0xD6 */ u8  flagD6;
    /* 0xD7 */ u8  flagD7;
    /* 0xD8 */ u8  flagD8;
    /* 0xD9 */ u8  flagD9;
    /* 0xDA */ u8  flagDA;
};
ASSERT_OFFSET(MixerLevels, gain00, 0x00);
ASSERT_OFFSET(MixerLevels, gainCC, 0xCC);
ASSERT_OFFSET(MixerLevels, flagD0, 0xD0);
ASSERT_OFFSET(MixerLevels, flagDA, 0xDA);
ASSERT_SIZE(MixerLevels, 0xDC);

void ClearMixerLevels(MixerLevels* m)
{
    m->flagD0 = 0;
    m->flagD1 = 0;
    m->flagD2 = 0;
    m->flagD3 = 0;
    m->flagD4 = 0;
    m->flagD5 = 0;
    m->flagD6 = 0;
    m->flagD7 = 0;
    m->flagD8 = 0;
    m->flagD9 = 0;
    m->flagDA = 0;

    m->gain00 = 0.0f;
    m->gain04 = 0.0f;
    m->gain08 = 0.0f;
    m->gain0C = 0.0f;
    m->gain10 = 0.0f;
    m->gain14 = 0.0f;
    m->gain18 = 0.0f;
    m->gain1C = 0.0f;
    m->gain20 = 0.0f;
    m->gain24 = 0.0f;
    m->gain28 = 0.0f;
    m->gain2C = 0.0f;
    m->gain30 = 0.0f;
    m->gain34 = 0.0f;
    m->gain38 = 0.0f;
    m->gain3C = 0.0f;
    m->gain40 = 0.0f;
    m->gain44 = 0.0f;
    m->gain48 = 0.0f;
    m->gain4C = 0.0f;
    m->gain50 = 0.0f;
    m->gain54 = 0.0f;
    m->gain58 = 0.0f;
    m->gain5C = 0.0f;
    m->gain60 = 0.0f;
    m->gain64 = 0.0f;
    m->gain68 = 0.0f;
    m->gain6C = 0.0f;
    m->gain70 = 0.0f;
    m->gain74 = 0.0f;
    m->gain78 = 0.0f;
    m->gain7C = 0.0f;
    m->gain80 = 0.0f;
    m->gain84 = 0.0f;
    m->gain88 = 0.0f;
    m->gain8C = 0.0f;
    m->gain90 = 0.0f;
    m->gain94 = 0.0f;
    m->gain98 = 0.0f;
    m->gain9C = 0.0f;
    m->gainA0 = 0.0f;
    m->gainA4 = 0.0f;
    m->gainA8 = 0.0f;
    m->gainAC = 0.0f;
    m->gainB0 = 0.0f;
    m->gainB4 = 0.0f;
    m->gainB8 = 0.0f;
    m->gainBC = 0.0f;
    m->gainC0 = 0.0f;
    m->gainC4 = 0.0f;
    m->gainC8 = 0.0f;
    m->gainCC = 0.0f;
}
