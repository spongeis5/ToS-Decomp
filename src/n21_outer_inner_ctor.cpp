// sub_82273AE8 -- constructor: a vtable at +0x000, a second vtable at
// +0x100, a self-referential buffer pointer, and 25 zeroed fields.
// 172 B, 4 callers.
//
//      lis  r11,-32255 ; addi r10,r3,256 ; lis r9,-32255
//      addi r8,r11,-2556       = 8200F604    vtable A
//      li   r11,0
//      addi r10,r10,64         this + 256 + 64
//      stw  r8,0(r3)           +0x000 = vtA
//      addi r7,r9,-4648        = 8200EDD8    vtable B
//      stw  r11,228(r3)        +0x0E4 = 0
//      li   r6,-1
//      stw  r11,232(r3)        +0x0E8 = 0
//      li   r5,2
//      stw  r10,276(r3)        +0x114 = this+320
//      lis  r4,-32256
//      stw  r7,256(r3)         +0x100 = vtB
//      stw  r6,268(r3)         +0x10C = -1
//      li   r10,1
//      stw  r5,272(r3)         +0x110 = 2
//      stb  r11,293(r3) ; sth r11,294(r3) ; stw r11,296(r3)
//      lfs  f0,11684(r4)       = 82002DA4 = 0.0f
//      stw  r11,300(r3) ; stw r11,304(r3) ; stw r11,312(r3)
//      stw  r11,384(r3) ; stw r11,376(r3) ; stw r11,380(r3)
//      sth  r11,388(r3) ; stb r11,390(r3)
//      stfs f0,28(r3)          +0x01C = 0.0f
//      stw  r11,96(r3)  ; stb r10,12(r3) ; stw r11,16(r3)
//      stb  r11,14(r3)  ; stb r11,13(r3)
//      stw  r11,52(r3)  ; stw r11,56(r3) ; stw r11,60(r3)
//      stw  r11,64(r3)  ; stw r11,48(r3) ; stb r11,15(r3)
//      blr
//
// THE SECOND VTABLE IS A MEMBER SUB-OBJECT, NOT A SECOND BASE, and the
// evidence is that there are exactly TWO vptr stores and the one at +0x000
// comes FIRST. Under multiple inheritance MSVC emits four -- each base ctor
// stores its own, then the most-derived class overwrites both -- and dead
// store elimination keeps the LATER pair, so both surviving stores would sit
// after every base ctor body. Here the +0x000 store is the sixth instruction
// in the function and the +0x100 store is thirteen instructions later with
// other fields between them. A class stores its own vptr before running any
// member initialiser, and a member sub-object's ctor stores its vptr at its
// own +0x00 where nothing overwrites it: outer vptr, then inner vptr, in that
// order, with no fifth or sixth store anywhere. So +0x100 holds an embedded
// polymorphic object.
//
// `addi r10,r3,256` FOLLOWED BY `addi r10,r10,64` IS THE PROOF, and it is the
// kind MATCHED.md warns is easy to read backwards. 320 fits a signed 16-bit
// immediate, so one `addi r10,r3,320` would do; two adds mean the address was
// built in two steps, from a base pointer that was materialised in its own
// right and then had a member offset applied. That is the inlined inner
// constructor taking `this = outer + 0x100` and forming `&buf40` at its own
// +0x40 -- the two-level shape MATCHED.md records for sub_82164040. Every
// plain member store folds back into `off(r3)`, because a constant-offset
// chain folds; only the address-of does not.
//
// So +0x114 is a pointer to +0x140, which is 0x14 and 0x40 inside the inner
// object: a small-buffer optimisation, the pointer aimed at the inline
// storage it will use until it has to grow.
//
// 0x82002DA4 is 0.0f, read out of the image -- the same pool word src/n8,
// src/n11 and src/n19 use.
//
// The two `li` constants that are not zero are -1 at +0x10C and 2 at +0x110,
// both inside the inner object, and 1 at +0x00C in the outer.
//
// Widths come from the stores: `stb` at 0x00C, 0x00D, 0x00E, 0x00F, 0x125 and
// 0x186; `sth` at 0x126 and 0x184; `stfs` at 0x01C; everything else `stw`
// from a zeroed GPR, which fixes four bytes and says nothing about the type.
//
// Store order is source order within each of the two streams (MATCHED.md's
// rule from sub_8214CCB8), so the odd sequences are written out as they are
// emitted: the inner object runs 0x14, 0x0C, 0x10, 0x25, 0x26, 0x28, 0x2C,
// 0x30, 0x38, 0x80, 0x78, 0x7C, 0x84, 0x86 -- note 0x80 ahead of 0x78 -- and
// the outer runs 0x60, 0x0C, 0x10, 0x0E, 0x0D, 0x34, 0x38, 0x3C, 0x40, 0x30,
// 0x0F.
//
// NEAR MISS: 22 of 37 compared words at /O2, 6 relocated and not compared.
// THE SIZE IS EXACT (172 of 172 bytes) and the whole body from +0x12C onward
// is word for word correct, including all eleven outer stores in their odd
// order. Everything that differs is in the PROLOGUE, and it is scheduling and
// register naming rather than a wrong field or a wrong constant:
//
//   * retail issues `lis`, `lis`, `addi`(vtA), `addi`(vtB) interleaved with
//     stores, and DELAYS the float pool's `lis r4` to the fourteenth
//     instruction; every shape tried hoists all three `lis`es to the front.
//   * retail stores vtA at +0x000 SEVENTH, ahead of +0x0E4 and +0x0E8; ours
//     puts +0x0E4 first.
//   * retail reuses r10 for both `this+320` and the constant 1 after the
//     store at +0x114 consumes it, where ours keeps six live constants in
//     six registers.
//
// That last one is MATCHED.md's /Os coalescing signature, and it is a FALSE
// lead here: /O2 /Os is 20 of 37, worse, and the same 172 bytes.
//
// THIRTEEN SHAPES MEASURED, all at both levels, none above 22 of 37:
//   outer -- +0x0E4/+0x0E8 as member initialisers (22, the best), as the
//   first body statements (12), as the last (2); the float store first (22),
//   in the middle (22), last (22).
//   inner -- `p14 = buf40` first (22), after the two constants (20), last
//   (15), as `&buf40[0]` (22), as a member initialiser (22), through a named
//   local (22), with the two constants as member initialisers (20), with all
//   three as member initialisers (20).
//
// So the remaining difference is not reachable from statement order or from
// mem-init-versus-body, which are the two axes a constructor offers. What has
// NOT been tried: a different decomposition of the outer object into base
// classes, which is the axis src/h5_dsp_ctor.cpp and src/n16_dsp_sink_ctor.cpp
// both turned on, and which would move where the vtA store sits relative to
// +0x0E4.
//
// The three lis/addi pairs and the lfs are relocated.

#include "types.h"

struct InnerBuf
{
    /* 0x00 */                        // vptr -- 8200EDD8
    /* 0x04 */ char  unk0004[0x08];
    /* 0x0C */ s32   f0C;
    /* 0x10 */ s32   f10;
    /* 0x14 */ char* p14;
    /* 0x18 */ char  unk0018[0x0D];
    /* 0x25 */ u8    f25;
    /* 0x26 */ u16   f26;
    /* 0x28 */ s32   f28;
    /* 0x2C */ s32   f2C;
    /* 0x30 */ s32   f30;
    /* 0x34 */ char  unk0034[0x04];
    /* 0x38 */ s32   f38;
    /* 0x3C */ char  unk003C[0x04];
    /* 0x40 */ char  buf40[0x38];
    /* 0x78 */ s32   f78;
    /* 0x7C */ s32   f7C;
    /* 0x80 */ s32   f80;
    /* 0x84 */ u16   f84;
    /* 0x86 */ u8    f86;
    /* 0x87 */ char  unk0087[0x01];

    InnerBuf();
    virtual void Slot0();
};
ASSERT_OFFSET(InnerBuf, f0C, 0x0C);
ASSERT_OFFSET(InnerBuf, p14, 0x14);
ASSERT_OFFSET(InnerBuf, f25, 0x25);
ASSERT_OFFSET(InnerBuf, f38, 0x38);
ASSERT_OFFSET(InnerBuf, buf40, 0x40);
ASSERT_OFFSET(InnerBuf, f80, 0x80);
ASSERT_OFFSET(InnerBuf, f86, 0x86);
ASSERT_SIZE(InnerBuf, 0x88);

InnerBuf::InnerBuf()
{
    p14 = buf40;
    f0C = -1;
    f10 = 2;
    f25 = 0;
    f26 = 0;
    f28 = 0;
    f2C = 0;
    f30 = 0;
    f38 = 0;
    f80 = 0;
    f78 = 0;
    f7C = 0;
    f84 = 0;
    f86 = 0;
}

struct OuterObj
{
    /* 0x000 */                        // vptr -- 8200F604
    /* 0x004 */ char     unk0004[0x08];
    /* 0x00C */ u8       f0C;
    /* 0x00D */ u8       f0D;
    /* 0x00E */ u8       f0E;
    /* 0x00F */ u8       f0F;
    /* 0x010 */ s32      f10;
    /* 0x014 */ char     unk0014[0x08];
    /* 0x01C */ f32      f1C;
    /* 0x020 */ char     unk0020[0x10];
    /* 0x030 */ s32      f30;
    /* 0x034 */ s32      f34;
    /* 0x038 */ s32      f38;
    /* 0x03C */ s32      f3C;
    /* 0x040 */ s32      f40;
    /* 0x044 */ char     unk0044[0x1C];
    /* 0x060 */ s32      f60;
    /* 0x064 */ char     unk0064[0x80];
    /* 0x0E4 */ s32      fE4;
    /* 0x0E8 */ s32      fE8;
    /* 0x0EC */ char     unk00EC[0x14];
    /* 0x100 */ InnerBuf inner;

    OuterObj();
    virtual void Slot0();
};
ASSERT_OFFSET(OuterObj, f0C, 0x00C);
ASSERT_OFFSET(OuterObj, f1C, 0x01C);
ASSERT_OFFSET(OuterObj, f30, 0x030);
ASSERT_OFFSET(OuterObj, f60, 0x060);
ASSERT_OFFSET(OuterObj, fE4, 0x0E4);
ASSERT_OFFSET(OuterObj, inner, 0x100);

OuterObj::OuterObj() : fE4(0), fE8(0)
{
    f60 = 0;
    f0C = 1;
    f10 = 0;
    f0E = 0;
    f0D = 0;
    f34 = 0;
    f38 = 0;
    f3C = 0;
    f40 = 0;
    f30 = 0;
    f0F = 0;

    f1C = 0.0f;
}
