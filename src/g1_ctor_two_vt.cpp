// sub_82784F90 -- construct an object over an inlined base: the base writes
// a vtable, zeroes a four-float block, issues an `lwsync` and sets four more
// words; the derived part zeroes the same four floats again, stores the
// second vtable and clears the rest. 140 B, 5 callers.  r4 = a u16.
//
//      lis/lis ; addi r9,r11,24024 = 82085DD8   (base vtable)
//      addi r11,r3,4                            DEAD -- never read
//      stw r9,0(r3) ; lfs f0,11684(r10) = 82002DA4 (0.0f)
//      stfs f0,4/8/12/16(r3)
//      lwsync                                   7C2004AC
//      li r10,1 ; li r11,0 ; stw r10,20(r3) ; addi r10,r9,24216 = 82085E98
//      stw r11,24/28/32(r3)
//      stfs f0,4/8/12/16(r3)                    the SAME four again
//      stb r11,36/37(r3) ; stw r10,0(r3)        the derived vtable
//      stw r11,40/44(r3) ; sth r11,48(r3) ; sth r4,50(r3)
//      stw r11,52/56/60(r3) ; sth r11,64(r3)
//
// TWO FACTS HOLD THIS TOGETHER.  The first four float stores survive only
// because `__lwsync()` is a memory barrier: without it MSVC's dead-store
// elimination removes them, since the second group overwrites the same four
// words.  And `__lwsync` must come from the XDK's own ppcintrinsics.h -- per
// MATCHED.md, declaring it by hand compiles silently and emits a real `bl`.
//
// The dead `addi r11,r3,4` is the inlined-helper fingerprint from
// src/t7_ctor_args5.cpp: the address of the float block is formed once,
// common-subexpressioned across both zeroing calls, and then folded away
// into r3-relative displacements, leaving the computation itself behind.
//
// MATCHED at /O2 /Os, 29 of 29, with the size exactly right at 140 bytes
// and 6 of the 35 words relocated and so not compared.
//
// What was wrong was ONE SCHEDULING WINDOW and nothing else in the function:
//
//   want  li r10,1 ; lis r9,vtB@ha ; li r11,0 ; stw r10,20 ; addi r10,r9 ; stw r11,24
//   got   lis r10,vtB@ha ; li r11,0 ; li r9,1 ; addi r10,r10 ; stw r11,24 ; stw r9,20
//
// The second vtable's high half was materialised one slot too early and the
// +20 store fell behind the +24 store as a result.
//
// THE ADDRESS-OF-MEMBER PIN FIXES IT:
//
//      s32* p = &it->f20;
//      *p = 1;
//
// Two constant offsets off one base provably cannot alias, so MSVC is free
// to issue the +24 store first and let the +20 store fall into the gap
// behind the vtable's `lis`/`addi`; taking the member's address removes that
// proof and the two stores stay in source order. Pinning the +24/+28/+32
// group through one `s32*` instead is equally 29 of 29 -- either side of the
// pair works, because it is the PAIR's freedom to swap that has to go. What
// does NOT work is routing f20 through f32 all through one pointer (25 of
// 29): that pins the group against the vtable store as well and puts the
// address computation back where it was.
//
// This is MATCHED.md's sub_827FEE48 lever used on a STORE, the third time in
// this batch -- sub_827007F8 and sub_825FE880 are the others, and all three
// were a store MSVC had slid into a scheduling gap.
//
// Also measured and NOT the answer: pinning the second vtable store itself
// (25 of 29), and moving that store up to sit right after `f20 = 1` (16 of
// 29). Before the pin, five placements of the vtable store were tried (after
// f37 25, between the Zero4 and f36 22, after f40/f44 22, before the second
// Zero4 16, last 16); the float block as a helper on either half, inline on
// either half, and a helper taking Item* instead of Quad*; the vtable
// through a named local; one vtable type instead of two; the base as its own
// inlined function; and both orders of the +20 store against the three
// zeroes. Then all 72 flag combinations `tools/flagsweep.py` builds: 28 give
// 25 of 35 and 44 give 24, none more -- so the flag axis was exhausted and
// the answer was a source shape after all.
//
// Writing the two float groups inline instead of through the helper is 136
// bytes -- dead-store elimination takes one of them, so the helper is what
// keeps both, not the `lwsync` alone.

#include "types.h"
#include <ppcintrinsics.h>

struct VTa;
struct VTb;
extern const VTa kVT_82085DD8;
extern const VTb kVT_82085E98;

struct Quad
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

static void Zero4(Quad* q)
{
    q->x = 0.0f;
    q->y = 0.0f;
    q->z = 0.0f;
    q->w = 0.0f;
}

struct Item
{
    /* 0x00 */ const VTa* vt;
    /* 0x04 */ Quad q;
    /* 0x14 */ s32  f20;
    /* 0x18 */ s32  f24;
    /* 0x1C */ s32  f28;
    /* 0x20 */ s32  f32;
    /* 0x24 */ u8   f36;
    /* 0x25 */ u8   f37;
    /* 0x26 */ char unk0026[2];
    /* 0x28 */ s32  f40;
    /* 0x2C */ s32  f44;
    /* 0x30 */ u16  f48;
    /* 0x32 */ u16  f50;
    /* 0x34 */ s32  f52;
    /* 0x38 */ s32  f56;
    /* 0x3C */ s32  f60;
    /* 0x40 */ u16  f64;
};
ASSERT_OFFSET(Item, q, 4);
ASSERT_OFFSET(Item, f36, 36);
ASSERT_OFFSET(Item, f50, 50);
ASSERT_OFFSET(Item, f64, 64);

void ConstructItem(Item* it, u16 kind)
{
    it->vt = (const VTa*)&kVT_82085DD8;
    Zero4(&it->q);
    __lwsync();
    s32* p = &it->f20;
    *p      = 1;
    it->f24 = 0;
    it->f28 = 0;
    it->f32 = 0;

    Zero4(&it->q);
    it->f36 = 0;
    it->f37 = 0;
    it->vt = (const VTa*)&kVT_82085E98;
    it->f40 = 0;
    it->f44 = 0;
    it->f48 = 0;
    it->f50 = kind;
    it->f52 = 0;
    it->f56 = 0;
    it->f60 = 0;
    it->f64 = 0;
}
