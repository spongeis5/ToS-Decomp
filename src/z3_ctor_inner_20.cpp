// sub_821AE548 -- the sibling of sub_821AE070: the same base constructor at
// 821A4628, the same sub-object constructor at 82202B50, and the same
// sub-object vtable 82004678, with the sub-object at +20 instead of +56.
// 132 B, BRIDGE between TypeId_821AE538 and TypeId_821AE5D0.
//
//      mflr / std r31 / stwu r1,-96(r1)
//      mr    r31,r3
//      bl    821A4628                  the base constructor, returns `this`
//      lis   r11
//      addi  r3,r3,20                  <- off the RETURNED pointer
//      addi  r10,r11,23184 = 82005A90  this class's vtable
//      stw   r10,0(r31)                <- off `this`: r3 is already the arg
//      bl    82202B50
//      lis r9 ; lis r8 ; lis r7 ; lis r6
//      addi  r5,r8,18040 = 82004678    the sub-object's vtable
//      li r3,1 ; lfs f0,12804(r9) = 82003204 (10.0f) ; li r4,0
//      stfs  f0,80(r31)                sub-object +60
//      stw   r5,20(r31)                sub-object vptr
//      lfs   f0,18276(r7) = 0.2f ; lfs f13,11684(r6) = 0.0f
//      stb   r3,120(r31) ; stfs f0,108(r31)
//      stw   r4,100(r31) ; stfs f13,112(r31)
//      mr    r3,r31
//
// THE SUB-OBJECT'S SIZE IS PINNED AT 80 BY THIS FUNCTION.  It starts at +20
// and the next field written is +100; 82202B50 writes as far as +0x4C, so 80
// is both the floor and the ceiling.  That in turn says the +152 store in
// sub_821AE070 -- where the same sub-object sits at +56, ending at 136 -- is
// an OUTER field and not a member of it.
//
// The quad {0, 0.2f, 0.0f, 1} recurs with identical spacing in both classes:
// +100/+108/+112/+120 here, +28/+36/+40/+48 there.
//
// Every store is off r31 because `addi r3,r3,20` claims r3 before the first
// of them; only the sub-object's address is taken off the returned pointer.
//
// THE TWO PINS ARE WHAT MATCH IT.  Written plainly the six stores after the
// sub-object's constructor come out reordered -- the zero at +100 hoisted to
// the front, and the +20 vtable store slid behind the cheap `stb` at +120 --
// at 13 of 21 and using three float registers where the image reloads f0.
// MATCHED.md's sub_827FEE48 lever on the two sub-object stores removes
// MSVC's no-alias proof and holds them in place: 17 of 21.  The remaining
// four words were the +100 and +120 stores transposed.
//
// Measured over 480 combinations -- every merge of the integer store stream
// with the float one, for all six orders of the integer stream, against four
// pin sets.  Twelve match, and every one of them has the +100 store BEFORE
// the +120 store and pins exactly the two sub-object stores; the order of
// those two against each other carries no information, and neither does
// where the +108 and +112 float stores fall.  So the four outer fields are
// written in offset order, which is the reading taken here.  /O2 only.

#include "types.h"

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_82005A90;

struct Inner
{
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ char       unk0004[0x38];
    /* 0x3C */ f32        f3C;
    /* 0x40 */ char       unk0040[0x10];
};
ASSERT_OFFSET(Inner, f3C, 0x3C);
ASSERT_SIZE(Inner, 80);

struct Outer2
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

    Outer2();
};
ASSERT_OFFSET(Outer2, inner, 0x14);
ASSERT_OFFSET(Outer2, f64,   0x64);
ASSERT_OFFSET(Outer2, f6C,   0x6C);
ASSERT_OFFSET(Outer2, f70,   0x70);
ASSERT_OFFSET(Outer2, f78,   0x78);

Outer2* BaseInit(Outer2* o);           /* sub_821A4628 -- returns its arg */
void    InnerBase(Inner* p);           /* sub_82202B50 */

Outer2::Outer2()
{
    Outer2* o = BaseInit(this);

    this->vt = (const VTc*)&kVT_82005A90;

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
