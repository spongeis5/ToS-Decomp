#include "types.h"

// sub_821ADFC8 -- MATCHED, 21 of 21 non-relocated words, /O2, 132 bytes.
// BRIDGE between TypeId_821ADFB8 and TypeId_821AE050 (244 bytes merged).
//
// It was recorded here as a five-word near miss whose residue was "the image
// reaches the object through the pointer the BASE CONSTRUCTOR RETURNED in r3
// and we reach it through the saved `this` in r31". That reading was right.
// The base-class spelling it was written in cannot express it -- MSVC does
// not model an out-of-line base constructor as returning `this` -- so the
// shape has to be the one the matched sibling sub_821AE548 uses:
//
//      Obj* o = BaseInit(this);
//
// with the stores addressed off `o` and the sub-object's address taken off
// `o` as well.
//
// THE LEVER THAT FINISHED IT is MATCHED.md's address-of pin (sub_827FEE48),
// applied to exactly ONE store -- the zero at +32 -- and to no other:
//
//      s32* p20 = &o->f20;  *p20 = 0;
//
// Without it the two float-constant loads issue one instruction LATE, after
// the integer store they should be interleaved with:
//
//      want  stw r8,0(r3) / lfs f0 / stw r7,32(r3) / lfs f13 / stb r6,52(r3)
//      got   stw r8,0(r3) / stw r7,32(r3) / lfs f0 / stb r6,52(r3) / lfs f13
//
// So the pin is not doing its usual job here (stopping a load being hoisted
// across a store); it is changing which store the scheduler can slide the
// two `lfs` past. The pin has to be on the ZERO store specifically, and the
// byte store and the 0.2f store must NOT be pinned -- pinning either of them
// as well costs six words.
//
// MEASURED, and worth not repeating:
//
//  * 120 statement orders (every permutation of the five pre-call stores),
//    with the vptr and byte stores pinned and the argument off `o`: ceiling
//    19 of 21, reached only by ABCDE and BACDE. 0 compile failures.
//  * 32 pin subsets x {ABCDE, BACDE} x {&o->inner, &this->inner} = 128
//    probes, 0 failures. Four match, all at order ABCDE with `&o->inner`:
//    pin sets {B}, {AB}, {BE}, {ABE}. Every set containing B but also C or D
//    scores 15 of 21; every set without B scores 5, 19 or 20.
//  * the argument spelling is worth one word here and there is no register
//    trade to pay for it, unlike sub_821AE070: this function has only ONE
//    callee-saved register. `&this->inner` caps at 20 of 21.
//
// Ruled out earlier, and still ruled out: every store through one pointer
// (14 of 21, the two store streams interleave one-for-one instead of three
// integer stores then two float ones); a real C++ base class with the vptr
// store in an intermediate class's inline constructor (16 of 21); the same
// with genuine virtual functions (identical output). /O2 /Os reorders the
// whole store group and is much worse in every spelling.
//
//      mflr / std r31 ; stwu r1,-96(r1)
//      mr    r31,r3
//      bl    821A4628                  the base constructor, returns its arg
//      lis r11 ; lis r10 ; lis r9
//      addi  r8,r11,21348 = 82005364   this class's vtable
//      li r7,0 ; li r6,1
//      stw   r8,0(r3)                  <- off the RETURNED pointer
//      lfs   f0,18276(r10) = 0.2f
//      stw   r7,32(r3)
//      lfs   f13,11684(r9) = 0.0f
//      stb   r6,52(r3)
//      stfs  f0,40(r3)
//      addi  r3,r3,60                  the sub-object's address
//      stfs  f13,44(r31)               <- off `this`: r3 is already the arg
//      bl    82202B50
//      lis r5 ; lis r4
//      addi  r3,r4,18040 = 82004678    the sub-object's vtable
//      lfs   f0,12804(r5) = 10.0f
//      stw   r3,60(r31) ; stfs f0,120(r31)
//      mr    r3,r31                    r3 live out: the return value is `this`
//
// The sub-object's size is 80, pinned by sub_821AE548 (src/z3_ctor_inner_20.cpp)
// where the same sub-object sits at +20 with the next field written at +100.
// Constants read out of the image: 82004764 = 0.2f, 82002DA4 = 0.0f,
// 82003204 = 10.0f.

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_82005364;

/* The 80-byte sub-object. Its base constructor is out of line at 82202B50. */
struct Inner
{
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ char       unk0004[0x38];
    /* 0x3C */ f32        f3C;
    /* 0x40 */ char       unk0040[0x10];
};
ASSERT_OFFSET(Inner, f3C, 0x3C);
ASSERT_SIZE(Inner, 80);

struct Obj821ADFC8
{
    /* 0x00 */ const VTc* vt;
    /* 0x04 */ char       unk0004[0x1C];
    /* 0x20 */ s32        f20;
    /* 0x24 */ char       unk0024[0x04];
    /* 0x28 */ f32        f28;
    /* 0x2C */ f32        f2C;
    /* 0x30 */ char       unk0030[0x04];
    /* 0x34 */ u8         f34;
    /* 0x35 */ char       unk0035[0x07];
    /* 0x3C */ Inner      inner;

    Obj821ADFC8();
};
ASSERT_OFFSET(Obj821ADFC8, f20,   0x20);
ASSERT_OFFSET(Obj821ADFC8, f28,   0x28);
ASSERT_OFFSET(Obj821ADFC8, f2C,   0x2C);
ASSERT_OFFSET(Obj821ADFC8, f34,   0x34);
ASSERT_OFFSET(Obj821ADFC8, inner, 0x3C);

Obj821ADFC8* BaseInit(Obj821ADFC8* o);   /* sub_821A4628 -- returns its arg */
void         InnerBase(Inner* p);        /* sub_82202B50 */

Obj821ADFC8::Obj821ADFC8()
{
    Obj821ADFC8* o = BaseInit(this);

    o->vt = (const VTc*)&kVT_82005364;
    s32* p20 = &o->f20;
    *p20 = 0;
    o->f34 = 1;
    o->f28 = 0.2f;
    this->f2C = 0.0f;

    InnerBase(&o->inner);

    this->inner.vt  = &kVT_82004678;
    this->inner.f3C = 10.0f;
}
