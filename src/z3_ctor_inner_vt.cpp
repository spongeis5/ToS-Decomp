// sub_821AE070 -- 23 of 24 non-relocated words, /O2, at exactly 144 bytes.
// ONE WORD WRONG, and it is one register operand:
//
//      want  38630038  addi r3,r3,56          the sub-object address off `o`
//      got   387f0038  addi r3,r31,56         ... off `this`
//
// Everything else -- prologue, both calls, all nine stores, both callee-saved
// registers, the epilogue -- is byte-identical.
//
// 144 B, 1 caller (821E2868).  BRIDGE: between TypeId_821AE060 and
// TypeId_821AE100, both matched, so a match merges a 748-byte run.  It is the
// sibling of sub_821AE548 (src/z3_ctor_inner_20.cpp, MATCHED): same base
// constructor 821A4628, same sub-object constructor 82202B50, same sub-object
// vtable 82004678, and the same quad {0, 0.2f, 0.0f, 1} at the same relative
// spacing -- +28/+36/+40/+48 here, +100/+108/+112/+120 there.
//
//      mflr / std r30 / std r31 ; stwu r1,-112(r1)
//      mr    r31,r3
//      bl    821A4628                  the base constructor
//      lis r11 ; lis r10 ; lis r9
//      addi  r8,r11,21428 = 820053B4   this class's vtable
//      li r7,1 ; li r30,0
//      stw   r8,0(r3)                  <- r3, NOT r31: the callee RETURNED it
//      lfs   f0,18276(r10) = 0.2f
//      stb   r7,48(r3)
//      lfs   f13,11684(r9) = 0.0f
//      stw   r30,28(r3)
//      stfs  f0,36(r3)
//      addi  r3,r3,56                  <- THE ONE WORD
//      stfs  f13,40(r31)               <- off `this`, so a second pointer
//      bl    82202B50
//      lis r6 ; lis r5 ; mr r3,r31     r3 live out: the return value is `this`
//      addi  r4,r5,18040 = 82004678
//      lfs   f0,12804(r6) = 10.0f
//      stw   r4,56(r31) ; stfs f0,116(r31) ; stw r30,152(r31)
//
// WHAT WAS MEASURED, and it is a real bind rather than a shape not yet tried.
//
// 1. THE BASE CALL'S RETURN VALUE IS USED.  r3 is volatile, yet the four
//    stores after `bl 821A4628` address off r3 and not off the r31 copy made
//    one instruction earlier.  Written as a real BASE CLASS with an
//    out-of-line constructor MSVC does NOT do this -- it uses r31 throughout
//    (13 of 24).  So 821A4628 is declared as returning the pointer and the
//    result is assigned; that is what puts r3 in those four stores.
//
// 2. TWO POINTERS ARE NEEDED, because +40 is stored off r31 while +0, +48,
//    +28 and +36 are off r3.  The compiler cannot know `o == this`, so a
//    store through one and a store through the other cannot be reordered --
//    and that ordering is the whole reason the middle block comes out in the
//    target's sequence.  With every store through one pointer MSVC reorders
//    freely: all 10 interleavings of the two store streams and all 6 tail
//    orders score identically at 13 of 24, so statement order alone carries
//    no information here.  Sweeping which pointer each of the five stores
//    uses (32 combinations x 2 sub-object arguments x 2 flag levels) lifts it
//    to 18 of 24.
//
// 3. THE ADDRESS-OF PIN IS THE SAME BARRIER WITHOUT THE REGISTER COST.
//    MATCHED.md's sub_827FEE48 lever applied to the +152 store -- `s32* q =
//    &this->f98; *q = 0;` -- stops MSVC hoisting it to the top of the tail
//    block and is worth 4 words, 18 -> 22.  Pinning the +0 and +48 stores as
//    well reaches 23.  (The same pin is what matched the sibling
//    sub_821AE548, on its +20 and +80 stores.)
//
// 4. THE LAST WORD IS A CALLEE-SAVED ASSIGNMENT, NOT A SHAPE.  Writing the
//    argument as `&o->inner` produces `addi r3,r3,56` correctly -- and then
//    r30 and r31 swap roles, `this` taking r30 and the zero r31, which is
//    wrong in eight words instead of one.  Every one of those eight is that
//    swap and nothing else.  So the two spellings each get half of it:
//
//        &this->inner   23 of 24   registers right, base wrong
//        &o->inner      16 of 24   base right, registers swapped
//
//    The flag axis is exhausted: tools/flagsweep.py's 72 combinations give
//    16 of 36 (44 of them, including plain /O2) or 13 of 36 (28, including
//    /O2 /Os) for the `&o->inner` shape -- none better, so no flag reaches
//    it.  Twelve source perturbations were measured on that shape and ALL
//    twelve score exactly 16: naming the zero in a local and using it at both
//    stores, hoisting `Inner* ip = &o->inner` to before or just after the
//    stores, a free function returning its parameter, a member function
//    returning `this`, a cast argument, `(Inner*)((char*)o + 56)`, the tail
//    stores through a named `Inner*`, `Outer* t = this` used for the
//    this-stores, declaring the +40 pin first, and clearing `o` after the
//    call.  All 32 pin subsets on that shape also cap at 16.
//
//    What has NOT been tried, and is where the next attempt should go: the
//    inlining axis, which MATCHED.md records as the one that reaches
//    register allocation when nothing else does -- the four stores in a
//    helper taking `Outer*`, that helper at two levels, and the sub-object's
//    call plus its two stores in a helper taking `Inner*`.
//
// Constants read out of the image: 82004764 = 0.2f, 82002DA4 = 0.0f,
// 82003204 = 10.0f.  The sub-object's size is 80, pinned by the sibling
// sub_821AE548 where it sits at +20 with the next field at +100 -- so +152
// here is an OUTER field and not a member of it.

#include "types.h"

struct VTa;
struct VTb;
struct VTc;

extern const VTa kVT_820053B4;
extern const VTb kVT_82004678;

struct Inner
{
    /* 0x00 */ const VTb* vt;
    /* 0x04 */ char       unk0004[0x38];
    /* 0x3C */ f32        f3C;
    /* 0x40 */ char       unk0040[0x10];
};
ASSERT_OFFSET(Inner, f3C, 0x3C);
ASSERT_SIZE(Inner, 80);

struct Outer
{
    /* 0x00 */ const VTc* vt;
    /* 0x04 */ char       unk0004[0x18];
    /* 0x1C */ s32        f1C;
    /* 0x20 */ char       unk0020[0x04];
    /* 0x24 */ f32        f24;
    /* 0x28 */ f32        f28;
    /* 0x2C */ char       unk002C[0x04];
    /* 0x30 */ u8         f30;
    /* 0x31 */ char       unk0031[0x07];
    /* 0x38 */ Inner      inner;
    /* 0x88 */ char       unk0088[0x10];
    /* 0x98 */ s32        f98;

    Outer();
};
ASSERT_OFFSET(Outer, f1C,   0x1C);
ASSERT_OFFSET(Outer, f24,   0x24);
ASSERT_OFFSET(Outer, f28,   0x28);
ASSERT_OFFSET(Outer, f30,   0x30);
ASSERT_OFFSET(Outer, inner, 0x38);
ASSERT_OFFSET(Outer, f98,   0x98);

Outer* BaseInit(Outer* o);             /* sub_821A4628 -- returns its arg */
void   InnerBase(Inner* p);            /* sub_82202B50 */

Outer::Outer()
{
    Outer* o = BaseInit(this);

    const VTc** pvt = &o->vt;
    *pvt = (const VTc*)&kVT_820053B4;
    u8* p30 = &o->f30;
    *p30 = 1;
    o->f1C = 0;
    o->f24 = 0.2f;
    f32* p28 = &this->f28;
    *p28 = 0.0f;

    InnerBase(&this->inner);

    this->inner.vt  = &kVT_82004678;
    this->inner.f3C = 10.0f;
    s32* q98 = &this->f98;
    *q98 = 0;
}
