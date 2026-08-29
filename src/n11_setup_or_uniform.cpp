// sub_82150E18 -- fill nine float fields, either from a settings block or
// from one uniform value. 128 B, 4 callers.
//
//      lwz  r11,4(r3)         o->src
//      lis  r10,-32256
//      lwz  r11,64(r11)       ->cfg, at +0x40
//      lfs  f0,11684(r10)     = 0x82002DA4, and that word is 0.0f
//      lbz  r9,40(r11)        cfg->enabled, +0x28
//      cmplwi cr6,r9,0
//      beq- cr6,uniform
//      lfs f13,44(r11) ; stfs f13,32(r3)     +0x20 = cfg->+0x2C
//      lfs f12,52(r11) ; stfs f12,36(r3)     +0x24 = cfg->+0x34
//      lfs f11,48(r11) ; stfs f11,24(r3)     +0x18 = cfg->+0x30
//      lfs f10,60(r11) ; stfs f10,40(r3)     +0x28 = cfg->+0x3C
//      lfs f9,56(r11)  ; stfs f9,44(r3)      +0x2C = cfg->+0x38
//      stfs f0,20(r3) ; stfs f0,48(r3) ; stfs f0,52(r3) ; stfs f0,56(r3)
//      blr
// uniform:
//      stfs f1,32(r3) ; stfs f1,36(r3) ; stfs f1,40(r3)
//      stfs f0,24(r3) ; stfs f0,44(r3)
//      stfs f0,20(r3) ; stfs f0,48(r3) ; stfs f0,52(r3) ; stfs f0,56(r3)
//      blr
//
// THE CONSTANT AT 0x82002DA4 IS 0.0f, read out of the image rather than
// guessed. A float zero has to come from the literal pool on this target --
// there is no GPR-to-FPR move -- so `lfs` here is not evidence of an
// interesting value. It is the same pool word src/n8_reset_two_floats.cpp
// uses, which is what a shared 0.0f looks like.
//
// f1 IS A FLOAT PARAMETER. The second arm stores it three times and never
// loads it, so it arrives in the first FP argument register.
//
// THE FOUR-STORE TAIL IS DUPLICATED INTO BOTH ARMS, in the same relative
// order, and each arm ends in its own `blr`. That is MSVC tail-duplicating
// code written ONCE after the if/else -- writing it out twice in the source
// would be the same bytes, so the shorter reading is the one taken; what the
// bytes do settle is that it comes AFTER both arms, because it is last in
// both copies.
//
// Every load/store in the first arm is a pair in emitted order and no load is
// hoisted over a store, which is the ordinary shape when `o` and `cfg` are
// different pointers the compiler cannot disambiguate. So the assignment
// order 0x20, 0x24, 0x18, 0x28, 0x2C is SOURCE order and not address order --
// worth writing down, because address order was the first guess and it is
// wrong here.
//
// The lis/lfs pair is relocated; the other 30 words are compared.

#include "types.h"

struct SetupCfg
{
    /* 0x00 */ char unk0000[0x28];
    /* 0x28 */ u8   enabled;
    /* 0x29 */ char unk0029[0x03];
    /* 0x2C */ f32  f2C;
    /* 0x30 */ f32  f30;
    /* 0x34 */ f32  f34;
    /* 0x38 */ f32  f38;
    /* 0x3C */ f32  f3C;
};
ASSERT_OFFSET(SetupCfg, enabled, 0x28);
ASSERT_OFFSET(SetupCfg, f3C, 0x3C);

struct SetupSrc
{
    /* 0x00 */ char            unk0000[0x40];
    /* 0x40 */ const SetupCfg* cfg;
};
ASSERT_OFFSET(SetupSrc, cfg, 0x40);

struct SetupDst
{
    /* 0x00 */ char             unk0000[0x04];
    /* 0x04 */ const SetupSrc*  src;
    /* 0x08 */ char             unk0008[0x0C];
    /* 0x14 */ f32              f14;
    /* 0x18 */ f32              f18;
    /* 0x1C */ char             unk001C[0x04];
    /* 0x20 */ f32              f20;
    /* 0x24 */ f32              f24;
    /* 0x28 */ f32              f28;
    /* 0x2C */ f32              f2C;
    /* 0x30 */ f32              f30;
    /* 0x34 */ f32              f34;
    /* 0x38 */ f32              f38;
};
ASSERT_OFFSET(SetupDst, src, 0x04);
ASSERT_OFFSET(SetupDst, f14, 0x14);
ASSERT_OFFSET(SetupDst, f20, 0x20);
ASSERT_OFFSET(SetupDst, f38, 0x38);

void SetupFields(SetupDst* o, float v)
{
    const SetupCfg* cfg = o->src->cfg;

    if (cfg->enabled)
    {
        o->f20 = cfg->f2C;
        o->f24 = cfg->f34;
        o->f18 = cfg->f30;
        o->f28 = cfg->f3C;
        o->f2C = cfg->f38;
    }
    else
    {
        o->f20 = v;
        o->f24 = v;
        o->f28 = v;
        o->f18 = 0.0f;
        o->f2C = 0.0f;
    }

    o->f14 = 0.0f;
    o->f30 = 0.0f;
    o->f34 = 0.0f;
    o->f38 = 0.0f;
}
