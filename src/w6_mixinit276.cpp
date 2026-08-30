#include "types.h"

// sub_82526340 -- wide audio-voice-shaped initialiser. 276 B, 4 callers.
//
// An 8-word stwu loop fills r3+192..223 with 0x3F800000 (1.0f as int --
// lis 16256 = 0x3F80), scattered zero stores, then value stores: -1 at 80,
// 1 at 56, 128 at 76, floats 1.0 (82002D40) at 148/108/112/160..188/264/
// 268/308/260/144/300, one value from 8205E3AC-7352 (8205C6B8) at 152,
// 0.0 (82002DA4) at 156, and a repeating constant 13604(r6)=82003544 at
// 336 and 272..292, plus pointer r3+96 stored at 96 and 100.
// Store ORDER is the source order throughout.

struct VoiceInit
{
    char pad0000[300];
};

extern const float kOne_82002D40;
extern const float kZero_82002DA4;
extern const float kA_8205C6B8;
extern const float kB_82003544;

void InitVoice(VoiceInit* v)
{
    s32 z = 0;
    *(s32*)((char*)v + 52) = z;
    *(s32*)((char*)v + 64) = z;
    *(s32*)((char*)v + 48) = z;
    *(s32*)((char*)v + 380) = z;
    for (int i = 0; i < 8; ++i)
        *(s32*)((char*)v + 192 + i * 4) = 0x3F800000;
    *(s32*)((char*)v + 40) = z;
    *(s32*)((char*)v + 256) = z;
    *(s32*)((char*)v + 360) = z;
    *(s32*)((char*)v + 364) = z;
    *(s32*)((char*)v + 68) = z;
    *(s32*)((char*)v + 80) = -1;
    *(float*)((char*)v + 148) = 1.0f;
    *(s32*)((char*)v + 56) = 1;
    *(float*)((char*)v + 108) = 1.0f;
    *(s32*)((char*)v + 76) = 128;
    *(float*)((char*)v + 112) = 1.0f;
    *(s32*)((char*)v + 116) = z;
    *(float*)((char*)v + 152) = kA_8205C6B8;
    *(s32*)((char*)v + 120) = z;
    *(float*)((char*)v + 156) = 0.0f;
    *(s32*)((char*)v + 124) = z;
    *(float*)((char*)v + 160) = 1.0f;
    *(float*)((char*)v + 164) = 1.0f;
    *(float*)((char*)v + 168) = 1.0f;
    *(float*)((char*)v + 172) = 1.0f;
    *(float*)((char*)v + 176) = 1.0f;
    *(float*)((char*)v + 180) = 1.0f;
    *(float*)((char*)v + 184) = 1.0f;
    *(float*)((char*)v + 188) = 1.0f;
    *(float*)((char*)v + 264) = 1.0f;
    *(float*)((char*)v + 268) = 1.0f;
    *(float*)((char*)v + 308) = 1.0f;
    *(float*)((char*)v + 336) = kB_82003544;
    *(float*)((char*)v + 260) = 1.0f;
    *(float*)((char*)v + 272) = kB_82003544;
    *(float*)((char*)v + 276) = kB_82003544;
    *(float*)((char*)v + 280) = kB_82003544;
    *(float*)((char*)v + 284) = kB_82003544;
    *(float*)((char*)v + 288) = kB_82003544;
    *(float*)((char*)v + 292) = kB_82003544;
    *(float*)((char*)v + 300) = 1.0f;
    *(float*)((char*)v + 304) = kB_82003544;
    *(float*)((char*)v + 144) = 1.0f;
    *(char**)((char*)v + 96) = (char*)v + 96;
    *(char**)((char*)v + 100) = (char*)v + 96;
    *(s32*)((char*)v + 104) = z;
}

// NEAR-MISS. loop of 1.0f-as-int constants plus scattered stores; order right, forms differ.
