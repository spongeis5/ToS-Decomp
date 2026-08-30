#include "types.h"

// sub_82103940 -- record a BOOL, and if the object is bound and its 4-bit
// format field at bits 16..19 of the word at 0x288C is one of four values,
// swap that field between its two variants and set dirty bit 54.
// 144 B.  Bridge between Acc_82103938 and Acc_821039D0.
//
//      lwz    r11,12452(r3) ; stw r4,12040(r3)   the load is hoisted ABOVE
//      cmplwi cr6,r11,0 ; beqlr cr6              the store: two constant
//                                                offsets cannot alias
//      lwz    r8,10380(r3) ; rlwinm r11,r8,16,28,31
//      cmplwi 2 / beq ; 3 / beq ; 10 / beq ; 12 / bnelr
//      rlwinm r10,r8,13,31,31 ; xor. r10,r10,r4 ; beqlr
//      rlwinm r9,r11,31,1,31 ; addi r10,r4,-1 ; addi r11,r11,3
//      not    r7,r10 ; addi r9,r9,-3
//      rlwinm r11,r11,17,0,14 ; rlwinm r7,r7,16,0,15
//      and    r10,r9,r10 ; and r11,r7,r11 ; rlwinm r10,r10,16,0,15
//      li     r12,1 ; or r11,r11,r10 ; rldicr r12,r12,54,63
//      rlwimi r11,r8,0,16,11 ; stw r11,10380(r3)
//      ld     r11,16(r3) ; or r11,r11,r12 ; std r11,16(r3)
//
// `addi r10,r4,-1` with `not` and the two `and`s is MSVC's branchless
// conditional: mask = v - 1 is 0 when v is 1 and all ones when v is 0, so
// `(a & ~mask) | (b & mask)` is `v ? a : b`. Both arms are shifted into
// place before the select, which is why the *2 in one arm shows up as a
// shift of 17 rather than 16.
//
// The two arms are inverses on the four accepted values: 2 -> 10 -> 2 and
// 3 -> 12 -> 3, which is what identifies (kind+3)*2 and (kind>>1)-3 rather
// than any other pair of expressions producing those numbers.
//
// The rotate that lands the tested bit at 31 (`rlwinm ...,13,31,31`) is bit
// 19 -- the TOP bit of the same 4-bit field -- so the early exit is "already
// in the requested state".
//
// `rlwimi r11,r8,0,16,11` has MB > ME, so the mask wraps: it keeps r8
// everywhere except bits 16..19, which come from the computed value.
//
// sub_821039D8 and sub_82103A70 are the same function on the next two
// stages -- offsets +4 and +8 throughout, dirty bits 53 and 52.
//
// THREE THINGS DECIDED THIS ONE.
//
// 1. The select is spelled with the MASK, not with `?:`. `v ? A : B` on an
//    integer compiles to a branch (cmpwi/beq/b, 128 bytes, 13 of 32); making
//    `v` a `bool` so MSVC knows it is 0 or 1 is worse still, because the
//    bool then costs a `clrlwi` on the store and turns the `xor.` into a
//    `cmplw`. Writing `m = v - 1` and `(A & ~m) | (B & m)` reproduces the
//    image's arithmetic word for word -- which is the honest reading, since
//    that mask is exactly what the instructions compute.
// 2. /O2 /Os. At plain /O2 the equality test splits into `xor` plus
//    `cmplwi cr6`, which is the same record-form-versus-split signature
//    MATCHED.md records for `clrlwi.` on sub_827156B8.
// 3. MATCHED.md's sub_827FEE48 ADDRESS-OF LEVER on the dirty word. Written
//    `d->dirty |= 1ULL << 54;` MSVC hoists the `ld` from offset 16 above the
//    store to offset 10380 -- two constant offsets off one base cannot alias
//    -- and every one of the last fifteen words shifts by an instruction.
//    `u64* pd = &d->dirty; *pd = *pd | ...;` pins it: 36 of 36.

struct FmtDev
{
    /* 0x0000 */ char  unk0000[0x10];
    /* 0x0010 */ u64   dirty;
    /* 0x0018 */ char  unk0018[0x288C - 0x18];
    /* 0x288C */ u32   fmt;
    /* 0x2890 */ char  unk2890[0x2F08 - 0x2890];
    /* 0x2F08 */ s32   value;
    /* 0x2F0C */ char  unk2F0C[0x30A4 - 0x2F0C];
    /* 0x30A4 */ void* bound;
};
ASSERT_OFFSET(FmtDev, dirty, 0x0010);
ASSERT_OFFSET(FmtDev, fmt,   0x288C);
ASSERT_OFFSET(FmtDev, value, 0x2F08);
ASSERT_OFFSET(FmtDev, bound, 0x30A4);

void FmtToggle0(FmtDev* d, u32 v)
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
            *pd = *pd | ((u64)1 << 54);
        }
    }
}
