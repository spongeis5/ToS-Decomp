#include "types.h"

// sub_821AE470 -- the fifth member of the sub_821AE548 constructor family,
// and instruction-for-instruction identical to sub_821AE340 and
// sub_821AE3D8 except for one word: this class's own vtable, 820059C4.
// 132 B, bridge between TypeId_821AE460 and TypeId_821AE4F8, for a 660-byte
// run.
//
//      mr    r31,r3 ; bl 821A4628
//      addi  r3,r3,20                  off the RETURNED pointer
//      addi  r10,r11,22980 = 820059C4
//      stw   r10,0(r31) ; bl 82202B50
//      addi  r5,r8,18040 = 82004678 ; li r3,1 ; li r4,0
//      stfs  f0,80(r31)  ; stw r5,20(r31)
//      stb   r3,120(r31) ; stfs f0,108(r31)
//      stw   r4,100(r31) ; stfs f13,112(r31)
//      mr    r3,r31
//
// Five classes now share this constructor body with only the vtable
// differing -- 82005A90, 8200599C, 82005974, 820059C4 -- so the layout is
// as well attested as anything in the image: an 80-byte sub-object at +20
// whose base constructor is out of line at 82202B50, and four fields at
// +100, +108, +112 and +120 holding 0, 0.2f, 0.0f and 1.
//
// The two levers are z3_ctor_inner_20.cpp's: the base initialiser declared
// to RETURN its argument (which is what puts `addi r3,r3,20` on the
// returned r3 rather than the saved r31), and MATCHED.md's sub_827FEE48
// address-of lever on the two sub-object stores.

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_820059C4;

struct Inner
{
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ char       unk0004[0x38];
    /* 0x3C */ f32        f3C;
    /* 0x40 */ char       unk0040[0x10];
};
ASSERT_OFFSET(Inner, f3C, 0x3C);
ASSERT_SIZE(Inner, 80);

struct Outer821AE470
{
    /* 0x00 */ const VTc* vt;
    /* 0x04 */ char       unk0004[0x10];
    /* 0x14 */ Inner      inner;
    /* 0x64 */ s32        f64;
    /* 0x68 */ char       unk0068[0x04];
    /* 0x6C */ f32        f6C;
    /* 0x70 */ f32        f70;
    /* 0x74 */ char       unk0074[0x04];
    /* 0x78 */ u8         f78;
    /* 0x79 */ char       unk0079[0x03];

    Outer821AE470();
};
ASSERT_OFFSET(Outer821AE470, inner, 0x14);
ASSERT_OFFSET(Outer821AE470, f64,   0x64);
ASSERT_OFFSET(Outer821AE470, f6C,   0x6C);
ASSERT_OFFSET(Outer821AE470, f70,   0x70);
ASSERT_OFFSET(Outer821AE470, f78,   0x78);

Outer821AE470* BaseInit(Outer821AE470* o);   /* sub_821A4628 -- returns its arg */
void           InnerBase(Inner* p);          /* sub_82202B50 */

Outer821AE470::Outer821AE470()
{
    Outer821AE470* o = BaseInit(this);

    this->vt = (const VTc*)&kVT_820059C4;

    InnerBase(&o->inner);

    const VTb** pvt = &this->inner.vt;
    *pvt = &kVT_82004678;
    f32* p3C = &this->inner.f3C;
    *p3C = 10.0f;

    this->f64 = 0;
    this->f6C = 0.2f;
    this->f70 = 0.0f;
    this->f78 = 1;
}
