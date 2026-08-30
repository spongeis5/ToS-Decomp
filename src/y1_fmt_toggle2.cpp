#include "types.h"

// sub_82103A70 -- sub_82103940 for the third stage. Instruction for
// instruction identical apart from four constants: the bound pointer at
// 0x30AC, the recorded value at 0x2F10, the packed word at 0x2894 and dirty
// bit 52. 144 B, bridge between Acc_82103A68 and Acc_82103B00.
//
// Three stages, three consecutive 144-byte bodies differing only in those
// four constants, is what makes the reading of the arithmetic in
// y1_fmt_toggle0.cpp safe: the four accepted field values 2, 3, 10 and 12
// and the two inverse transforms (kind+3)*2 and (kind>>1)-3 are the same in
// all three.

struct FmtDev2
{
    /* 0x0000 */ char  unk0000[0x10];
    /* 0x0010 */ u64   dirty;
    /* 0x0018 */ char  unk0018[0x2894 - 0x18];
    /* 0x2894 */ u32   fmt;
    /* 0x2898 */ char  unk2898[0x2F10 - 0x2898];
    /* 0x2F10 */ s32   value;
    /* 0x2F14 */ char  unk2F14[0x30AC - 0x2F14];
    /* 0x30AC */ void* bound;
};
ASSERT_OFFSET(FmtDev2, dirty, 0x0010);
ASSERT_OFFSET(FmtDev2, fmt,   0x2894);
ASSERT_OFFSET(FmtDev2, value, 0x2F10);
ASSERT_OFFSET(FmtDev2, bound, 0x30AC);

void FmtToggle2(FmtDev2* d, u32 v)
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
            *pd = *pd | ((u64)1 << 52);
        }
    }
}
