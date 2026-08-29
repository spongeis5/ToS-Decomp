// sub_82250DE0 -- copy a 16-byte constant into a field and clear a word
// after it. 56 bytes, 3 callers.
//
//      lis  r11,-32256 ; li r10,0
//      addi r9,r11,11292          -> 82002C1C
//      addi r8,r3,44              <- COMPUTED AND NEVER READ
//      lwz  r7,11292(r11) ; stw r7,44(r3)
//      lwz  r6,4(r9)      ; stw r6,48(r3)
//      lwz  r5,8(r9)      ; stw r5,52(r3)
//      lwz  r4,12(r9)     ; stw r4,56(r3)
//      stw  r10,68(r3)
//      blr
//
// The constant is {0.0f, 0.0f, 0.0f, 1.0f}, read out of the image -- an
// identity quaternion -- and it is copied by WORDS, which is what MSVC does
// for a small POD assignment whatever the member types are.
//
// `addi r8,r3,44` is a DEAD address computation: its result is never read,
// because every store uses the displacement off r3 instead. Per the
// sub_82164040 / sub_82703E28 lever that is what an inlined helper taking
// `&member` leaves behind, and it needs TWO levels of inlining to survive --
// a flat body folds the base into r3 and the `addi` disappears.
//
// The global's first word folds its low half into the `lwz`; words 1 to 3
// need the address in a register first, because a relocated immediate will
// not combine with a constant.
//
// 3 of 14 words are relocated.

#include "types.h"

struct Quat
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

ASSERT_SIZE(Quat, 16);

extern const Quat g_identityQuat;

struct Node2D
{
    /* 0x00 */ u8   unk0000[0x2C];
    /* 0x2C */ Quat rotation;
    /* 0x3C */ u8   unk003C[8];
    /* 0x44 */ s32  dirty;
};

ASSERT_OFFSET(Node2D, rotation, 44);
ASSERT_OFFSET(Node2D, dirty, 68);

static void StoreQuat(Quat* dst, const Quat* src)
{
    *dst = *src;
}

static void ResetRotation(Node2D* n)
{
    StoreQuat(&n->rotation, &g_identityQuat);
}

void ResetNode(Node2D* n)
{
    ResetRotation(n);
    n->dirty = 0;
}
