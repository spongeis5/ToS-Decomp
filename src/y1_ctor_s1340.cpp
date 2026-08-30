#include "types.h"

// sub_821D9A78 -- constructor: an out-of-line base, this class's own vtable
// pointer, and one 28-byte sub-object at +1340. 100 B.
// Bridge between Acc_821D9A68 and TypeId_821D9AE0.
//
//      mr   r31,r3 ; bl 0x821d9948          the base constructor
//      addi r8,r11,-26632   -> 820097F8     this class's vtable
//      li   r7,0 ; li r6,1
//      stw  r8,0(r31)                       vptr, AFTER the base ctor and
//                                           BEFORE any member ctor
//      stw  r7,1340(r31)                    s.a = 0
//      stb  r6,1360(r31)                    s.d = 1
//      stfs f0,1348(r31)                    s.b = 0.2f     (82004764)
//      stfs f13,1352(r31)                   s.c = 0.0f     (82002DA4)
//      mr   r3,r31                          a ctor returns `this`
//
// THE SUB-OBJECT IS THE SAME ONE FOUR CONSTRUCTORS BUILD. Fields at +0, +8,
// +12 and +20 with the values 0, 0.2f, 0.0f and 1 appear at +552 and +1340
// here, at +28 in sub_821AE070, at +32 in sub_821ADFC8 and at +100 in
// sub_821AE3D8; the last two pin its size at 28, because the next member
// starts exactly 28 bytes later in both.
//
// The vptr store sits between the base constructor and the member
// constructors, which is where MSVC puts it and NOT where a plain assignment
// in the body would land -- a body assignment runs after every member ctor.
// So it is written as the body of an intermediate class's inline
// constructor, which is the position C++ gives a vptr without needing the
// compiler to emit a vftable of its own.
//
// The two integer stores come out in source order (a then d) and the two
// float stores in theirs (b then c), interleaved one for one -- the two
// store streams of MATCHED.md.

struct VT821D9A78;
extern const VT821D9A78 kVTable_820097F8;

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

/* The base whose constructor is sub_821D9948. Its size is fixed by where the
 * derived class's first member lands: 1340. */
struct Base821D9948
{
    Base821D9948();
    /* 0x000 */ const VT821D9A78* vt;
    /* 0x004 */ u8 unk0004[1340 - 4];
};
ASSERT_SIZE(Base821D9948, 1340);

struct Vt821D9A78 : Base821D9948
{
    Vt821D9A78() { vt = &kVTable_820097F8; }
};

struct Obj821D9A78 : Vt821D9A78
{
    Sub28 s;

    Obj821D9A78();
};

Obj821D9A78::Obj821D9A78()
{
}
