#include "types.h"

// sub_821ADFC8 -- NEAR MISS, 5 of 33 words, and all five are the same
// question. 132 B, bridge between TypeId_821ADFB8 and TypeId_821AE050.
//
// 16 of 33 words identical, 5 differ, 12 are relocated and excluded. Every
// instruction, every offset, every immediate and the whole schedule are
// right; the five wrong words are
//
//      want stw  r8,0(r3)      got stw  r8,0(r31)
//      want stw  r7,32(r3)     got stw  r7,32(r31)
//      want stb  r6,52(r3)     got stb  r6,52(r31)
//      want stfs f0,40(r3)     got stfs f0,40(r31)
//      want addi r3,r3,60      got addi r3,r31,60
//
// i.e. the image reaches the object through the pointer the BASE
// CONSTRUCTOR RETURNED in r3, and we reach it through the saved `this` in
// r31. The sixth store, `stfs f13,44(r31)`, is off r31 in BOTH -- it is
// scheduled after `addi r3,r3,60` has consumed the returned pointer, which
// is exactly why it has to be.
//
// THIS IS THE SAME RESIDUE AS sub_821AE070, and src/z3_ctor_inner_vt.cpp
// carries the full measurement of it -- 40-odd source shapes, all 720
// statement orders and all 72 flag combinations. Its finding 8 is the one
// that matters here and is a fact about the compiler rather than about
// either function: **MSVC does not model an out-of-line base constructor as
// returning `this`**, so it will not address a member off r3 after
// `bl Base::Base()`. The four r3 stores therefore need a return value the
// SOURCE uses, i.e. `Outer* o = BaseInit(this);`.
//
// What is new here, and is why this file keeps the base-class spelling
// rather than that one: sub_821AE070 splits exactly in half between
// `&this->inner` (registers right, base wrong, 23 of 24) and `&o->inner`
// (base right, r30/r31 swapped, 16 of 24). sub_821ADFC8 has only ONE
// callee-saved register, so there is no swap to trade against -- and the
// trade does not appear. Measured, both at /O2:
//
//  * `Outer* o = BaseInit(this)` with the four stores and the vptr through
//    `o` and `InnerBase(&o->inner)`: registers right, but the two store
//    streams interleave differently and the sixth store is 44(r3) rather
//    than 44(r31). 7 words.
//  * the same with the sixth store through `this` -- in three positions,
//    and with the sub-object pointer named in a local: MSVC splits `o`'s
//    live range with an `mr r11,r3` and the body grows to 136 bytes.
//  * the same plus z3_ctor_inner_vt.cpp's address-of pins on `&o->vt` and
//    `&o->f34`: 148 bytes with `&o->inner` (the `addi` is hoisted above the
//    first call) and 160 bytes with `&this->inner` (the pins each take a
//    register). 6 and 2 words.
//  * a real C++ base class with the vptr store in an intermediate class's
//    inline constructor -- this file, and the best of them at 5 words;
//  * the same with genuine virtual functions so the vptr store is the
//    compiler's own: identical output, still r31.
//
// /O2 /Os is much worse in every spelling: it reorders the whole store
// group.

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_82005364;

struct Sub28
{
    /* 0x00 */ s32 a;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ f32 b;
    /* 0x0C */ f32 c;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8  d;
    /* 0x15 */ u8  unk15[7];

    Sub28()
    {
        a = 0;
        b = 0.2f;
        c = 0.0f;
        d = 1;
    }
};
ASSERT_OFFSET(Sub28, b, 0x08);
ASSERT_OFFSET(Sub28, c, 0x0C);
ASSERT_OFFSET(Sub28, d, 0x14);
ASSERT_SIZE(Sub28, 28);

/* The 80-byte sub-object, size pinned by sub_821AE548: it sits at +20 there
 * and the next field written is +100. Its base constructor is out of line at
 * 82202B50 and the derived half is inline. */
struct BaseM82202B50
{
    BaseM82202B50();
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ u8  unk04[0x38];
    /* 0x3C */ f32 f3C;
    /* 0x40 */ u8  unk40[0x10];
};
ASSERT_OFFSET(BaseM82202B50, f3C, 0x3C);
ASSERT_SIZE(BaseM82202B50, 80);

struct Sub80 : BaseM82202B50
{
    Sub80()
    {
        vt  = &kVT_82004678;
        f3C = 10.0f;
    }
};

struct BaseA821A4628
{
    BaseA821A4628();
    /* 0x00 */ const VTc* vt;
    /* 0x04 */ u8 unk04[28];
};
ASSERT_SIZE(BaseA821A4628, 32);

struct VtObj821ADFC8 : BaseA821A4628
{
    VtObj821ADFC8() { vt = &kVT_82005364; }
};

struct Obj821ADFC8 : VtObj821ADFC8
{
    Sub28 s;   /* 0x20 */
    Sub80 m;   /* 0x3C */

    Obj821ADFC8();
};

Obj821ADFC8::Obj821ADFC8()
{
}
