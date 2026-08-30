#include "types.h"

// sub_82103008, sub_82103028, sub_82103048, sub_82103068, sub_82103088 --
// five 20-byte state setters, each storing one byte and setting one bit in
// the 64-bit dirty word at +0x10. Each is a BRIDGE between two of the
// Acc_82103xxx accessors in vt_acc_01.cpp.
//
//      stb r4,10498(r3)
//      ld  r11,16(r3) ; oris r11,r11,4096 ; std r11,16(r3)
//
// `oris ...,4096` is `|= 0x10000000`, i.e. bit 28; `oris ...,8192` is bit 29.
// Both fit in the low half of the doubleword, so no constant has to be
// materialised -- which is what distinguishes these from the float setters
// in y1_float_state.cpp, where the bit is above 31 and needs an `li` plus
// `rldicr`.
//
// TWO THINGS, and a BYTE STORE DOES NOT STOP EITHER.
//
// 1. MATCHED.md's sub_827FEE48 address-of lever is needed on the dirty word
//    exactly as in y1_fmt_toggle0.cpp. Written `d->dirty |= ...` MSVC hoists
//    the `ld` from +0x10 above the `stb` and the two come out transposed. A
//    byte store aliasing everything is what sub_82663370 needed to explain a
//    RELOAD of a pointer field; it does not stop the compiler moving a load
//    of a different constant offset off the same base, which is the
//    disambiguation this depends on.
// 2. /O2 /Os. At plain /O2 the OR gets a fresh destination register where
//    the image reuses the one the `ld` loaded into -- MATCHED.md's
//    register-coalescing signature, and the last two words of all five.
//
// The five bytes are consecutive and the bits are shared -- 0x2902 and
// 0x2901 both set bit 28, and 0x28FF, 0x28FE and 0x28FD all set bit 29 --
// so this is one register's worth of state split across two dirty flags.

struct ByteState
{
    /* 0x0000 */ char unk0000[0x10];
    /* 0x0010 */ u64  dirty;
    /* 0x0018 */ char unk0018[0x28FD - 0x18];
    /* 0x28FD */ u8   b28FD;
    /* 0x28FE */ u8   b28FE;
    /* 0x28FF */ u8   b28FF;
    /* 0x2900 */ u8   b2900;
    /* 0x2901 */ u8   b2901;
    /* 0x2902 */ u8   b2902;
};
ASSERT_OFFSET(ByteState, dirty, 0x0010);
ASSERT_OFFSET(ByteState, b28FD, 0x28FD);
ASSERT_OFFSET(ByteState, b2902, 0x2902);

void SetByte2902(ByteState* d, u8 v)
{
    d->b2902 = v;
    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 28);
}

void SetByte2901(ByteState* d, u8 v)
{
    d->b2901 = v;
    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 28);
}

void SetByte28FF(ByteState* d, u8 v)
{
    d->b28FF = v;
    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 29);
}

void SetByte28FE(ByteState* d, u8 v)
{
    d->b28FE = v;
    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 29);
}

void SetByte28FD(ByteState* d, u8 v)
{
    d->b28FD = v;
    u64* pd = &d->dirty;
    *pd = *pd | ((u64)1 << 29);
}
