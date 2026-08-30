#include "types.h"

// sub_82103C98, sub_82103CC8, sub_82103CF8, sub_82103D20 -- four state
// setters that take the value as a DWORD and store it as a FLOAT, then set
// one bit in the second 64-bit dirty word at +0x20. Each is a BRIDGE
// between two Acc_82103xxx accessors.
//
//      stw  r4,28(r1) ; lfs f0,28(r1) ; stfs f0,10700(r3)
//      li   r12,1 ; ld r10,32(r3) ; rldicr r12,r12,32,63
//      or   r11,r10,r12 ; std r11,32(r3)
//
// The store-then-reload through the stack is the argument being
// REINTERPRETED, not converted: an `int`-to-`float` conversion is `std`/
// `lfd`/`fcfid`/`frsp`, and nothing here converts. So the parameter is a
// DWORD holding a float's bits, which is the D3D render-state convention.
//
// The bit is above 31 in three of the four, so it has to be materialised
// (`li r12,1` plus `rldicr`); sub_82103CF8's bit 31 fits an `oris` and is
// four bytes shorter. Nothing in the source distinguishes them -- the
// compiler picks -- which is what makes the four a single shape.
//
// Both of y1_byte_state.cpp's levers apply unchanged: MATCHED.md's
// sub_827FEE48 address-of lever on the dirty word, without which the `ld`
// from +0x20 is hoisted above the `stfs`, and /O2 /Os for the coalesced
// destination of the OR.

struct FloatState
{
    /* 0x0000 */ char unk0000[0x20];
    /* 0x0020 */ u64  dirty2;
    /* 0x0028 */ char unk0028[0x29C4 - 0x28];
    /* 0x29C4 */ f32  f29C4;
    /* 0x29C8 */ f32  f29C8;
    /* 0x29CC */ f32  f29CC;
    /* 0x29D0 */ f32  f29D0;
};
ASSERT_OFFSET(FloatState, dirty2, 0x0020);
ASSERT_OFFSET(FloatState, f29C4,  0x29C4);
ASSERT_OFFSET(FloatState, f29D0,  0x29D0);

void SetFloat29CC(FloatState* d, u32 v)
{
    d->f29CC = *(f32*)&v;

    u64* pd = &d->dirty2;
    *pd = *pd | ((u64)1 << 32);
}

void SetFloat29C4(FloatState* d, u32 v)
{
    d->f29C4 = *(f32*)&v;

    u64* pd = &d->dirty2;
    *pd = *pd | ((u64)1 << 34);
}

void SetFloat29D0(FloatState* d, u32 v)
{
    d->f29D0 = *(f32*)&v;

    u64* pd = &d->dirty2;
    *pd = *pd | ((u64)1 << 31);
}

void SetFloat29C8(FloatState* d, u32 v)
{
    d->f29C8 = *(f32*)&v;

    u64* pd = &d->dirty2;
    *pd = *pd | ((u64)1 << 33);
}
