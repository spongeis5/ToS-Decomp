#include "types.h"

// sub_821AE070 -- NEAR MISS, 19 of 24 non-relocated words, /O2.  144 B, and
// the HIGHEST-VALUE bridge in the image: it joins TypeId_821AE060 and
// TypeId_821AE100, both of which are in this class's own vtable at 820053B4
// (slots 1 and 11), for a 748-byte run.
//
// THIS READING IS DOMINATED AND IS KEPT ONLY FOR ITS LAYOUT.  The returned-
// pointer reading in src/z3_ctor_inner_vt.cpp is 23 of 24 on the same
// address, and the base-class spelling used here is now known to be
// structurally unable to reach the four r3 stores: MSVC does not model an
// out-of-line base constructor as returning `this`.  That was measured
// twice.  Once as finding 8 in z3_ctor_inner_vt.cpp, and once by
// sub_821ADFC8 (src/y1_ctor_s32_m60.cpp), which was written in exactly this
// base-class spelling with exactly these five wrong words and MATCHED as
// soon as it was rewritten as `Obj* o = BaseInit(this)` with one address-of
// pin.  So the five words below are not a scheduling residue to be chased in
// this shape; they are the shape.
//
// 19 of 36 words identical, 5 differ, 12 are relocated and excluded. The
// five:
//
//      want stw  r8,0(r3)      got stw  r8,0(r31)
//      want stb  r7,48(r3)     got stb  r7,48(r31)
//      want stw  r30,28(r3)    got stw  r30,28(r31)
//      want stfs f0,36(r3)     got stfs f0,36(r31)
//      want addi r3,r3,56      got addi r3,r31,56
//
// -- the image reaches the object through the pointer the BASE CONSTRUCTOR
// RETURNED in r3, we reach it through the saved `this` in r31. The store
// after `addi r3,r3,56` (`stfs f13,40(r31)`) is off r31 in both, and the
// whole schedule, both store streams, the r30 allocation for the twice-used
// zero and every immediate are already right. src/z3_ctor_inner_vt.cpp
// carries the full measurement of this residue -- 60-odd source shapes, all
// 720 statement orders, all 256 pin subsets and 2334 flag combinations --
// and src/y1_ctor_s32_m60.cpp carries the shape that finished the sibling.
//
// The layout is settled and is worth keeping even though the function is
// not matched: 56 + 80 = 136 puts the +152 store outside the 80-byte
// sub-object, which is what sub_821AE548 established by placing the same
// sub-object at +20 with the next field at +100. The zero being used twice
// -- at +28 and at +152 -- is why it goes to the non-volatile r30 and why
// the byte store at +48 issues before it, where sub_821ADFC8, the otherwise
// identical shape with one use, emits +32 before +52.

struct VTb;
struct VTc;

extern const VTb kVT_82004678;
extern const VTc kVT_820053B4;

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
    /* 0x04 */ u8 unk04[24];
};
ASSERT_SIZE(BaseA821A4628, 28);

struct VtObj821AE070 : BaseA821A4628
{
    VtObj821AE070() { vt = &kVT_820053B4; }
};

struct Obj821AE070 : VtObj821AE070
{
    Sub28 s;          /* 0x1C */
    Sub80 m;          /* 0x38 */
    u8    unk88[16];  /* 0x88 */
    s32   f98;        /* 0x98 */

    Obj821AE070();
};

Obj821AE070::Obj821AE070()
{
    f98 = 0;
}
