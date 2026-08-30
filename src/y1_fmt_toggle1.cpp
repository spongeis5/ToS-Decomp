#include "types.h"

// sub_821039D8 -- sub_82103940 for the second stage. Instruction for
// instruction identical apart from four constants: the bound pointer at
// 0x30A8, the recorded value at 0x2F0C, the packed word at 0x2890 and dirty
// bit 53. 144 B, bridge between Acc_821039D0 and Acc_82103A68.
//
// See y1_fmt_toggle0.cpp for the three levers this needs: the select written
// as an explicit `v - 1` mask rather than `?:`, /O2 /Os for the record-form
// `xor.`, and MATCHED.md's address-of lever on the dirty word to stop the
// `ld` at offset 16 being hoisted above the store at 0x2890.

struct FmtDev1
{
    /* 0x0000 */ char  unk0000[0x10];
    /* 0x0010 */ u64   dirty;
    /* 0x0018 */ char  unk0018[0x2890 - 0x18];
    /* 0x2890 */ u32   fmt;
    /* 0x2894 */ char  unk2894[0x2F0C - 0x2894];
    /* 0x2F0C */ s32   value;
    /* 0x2F10 */ char  unk2F10[0x30A8 - 0x2F10];
    /* 0x30A8 */ void* bound;
};
ASSERT_OFFSET(FmtDev1, dirty, 0x0010);
ASSERT_OFFSET(FmtDev1, fmt,   0x2890);
ASSERT_OFFSET(FmtDev1, value, 0x2F0C);
ASSERT_OFFSET(FmtDev1, bound, 0x30A8);

void FmtToggle1(FmtDev1* d, u32 v)
{
    d->value = (s32)v;

    if (d->bound == 0)
        return;

    u32 w    = d->fmt;
    u32 kind = (w >> 16) & 0xF;

    if (kind == 2 || kind == 3 || kind == 10 || kind == 12)
    {
        if ((((w >> 19) & 1) ^ v) != 0)
        {
            u32 m  = v - 1;
            u32 nk = (((kind + 3) * 2) & ~m) | (((kind >> 1) - 3) & m);

            d->fmt = (w & 0xFFF0FFFFu) | ((nk << 16) & 0x000F0000u);

            u64* pd = &d->dirty;
            *pd = *pd | ((u64)1 << 53);
        }
    }
}
