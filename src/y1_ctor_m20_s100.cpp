#include "types.h"

// sub_821AE3D8 -- the third member of the sub_821AE070 / sub_821AE548 family
// and instruction-for-instruction the twin of sub_821AE548: the same base
// constructor at 821A4628, the same sub-object constructor at 82202B50, the
// same sub-object at +20 with vtable 82004678, and the same four outer
// fields at +100/+108/+112/+120. The ONLY difference between the two
// functions is this class's own vtable, 8200599C against 82005A90.
// 132 B, bridge between TypeId_821AE3C8 and TypeId_821AE460.
//
//      mr    r31,r3 ; bl 821A4628
//      addi  r3,r3,20                  off the RETURNED pointer
//      addi  r10,r11,22940 = 8200599C
//      stw   r10,0(r31) ; bl 82202B50
//      addi  r5,r8,18040 = 82004678 ; li r3,1 ; li r4,0
//      stfs  f0,80(r31)  ; stw r5,20(r31)
//      stb   r3,120(r31) ; stfs f0,108(r31)
//      stw   r4,100(r31) ; stfs f13,112(r31)
//      mr    r3,r31
//
// So the two levers z3_ctor_inner_20.cpp measured carry straight over, and
// they are the whole of it:
//
//  * the base initialiser is declared to RETURN its argument, which is what
//    lets `addi r3,r3,20` come off the returned r3 rather than off the saved
//    r31. A real base-class constructor does not do this -- neither with an
//    intermediate class's inline constructor holding the vptr store nor with
//    genuine virtual functions, both of which emit these same instructions
//    with r31 in that one word;
//  * MATCHED.md's sub_827FEE48 address-of lever on the two sub-object
//    stores, without which MSVC's no-alias proof lets them drift.

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_8200599C;

struct Inner
{
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ char       unk0004[0x38];
    /* 0x3C */ f32        f3C;
    /* 0x40 */ char       unk0040[0x10];
};
ASSERT_OFFSET(Inner, f3C, 0x3C);
ASSERT_SIZE(Inner, 80);

struct Outer821AE3D8
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

    Outer821AE3D8();
};
ASSERT_OFFSET(Outer821AE3D8, inner, 0x14);
ASSERT_OFFSET(Outer821AE3D8, f64,   0x64);
ASSERT_OFFSET(Outer821AE3D8, f6C,   0x6C);
ASSERT_OFFSET(Outer821AE3D8, f70,   0x70);
ASSERT_OFFSET(Outer821AE3D8, f78,   0x78);

Outer821AE3D8* BaseInit(Outer821AE3D8* o);   /* sub_821A4628 -- returns its arg */
void           InnerBase(Inner* p);          /* sub_82202B50 */

Outer821AE3D8::Outer821AE3D8()
{
    Outer821AE3D8* o = BaseInit(this);

    this->vt = (const VTc*)&kVT_8200599C;

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
