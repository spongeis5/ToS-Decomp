// sub_825DEB20 -- constructor: one vtable, two circular list sentinels, a
// 1.0f and a 0.0f, and eleven zeroed fields. 132 B, 4 callers.
//
//      lis  r10,-32250 ; lis r8,-32256 ; lis r7,-32256 ; li r11,0
//      addi r9,r3,4
//      addi r6,r10,13264      = 820633D0     the vtable
//      stw  r11,12(r3)        +0x0C = 0
//      addi r10,r3,52         this+52
//      stw  r9,4(r3)          +0x04 = this+4
//      stw  r9,8(r3)          +0x08 = this+4
//      lfs  f0,11584(r8)      = 82002D40 = 1.0f
//      stw  r6,0(r3)          vptr
//      lfs  f13,11684(r7)     = 82002DA4 = 0.0f
//      stw  r10,52(r3)        +0x34 = this+52
//      stw  r10,56(r3)        +0x38 = this+52
//      stw  r11,60(r3)        +0x3C = 0
//      stfs f0,32(r3)         +0x20 = 1.0f
//      stfs f13,36(r3)        +0x24 = 0.0f
//      stw  r11,20(r3)  ; stw r11,24(r3)      +0x14, +0x18
//      stb  r11,28(r3)  ; stb r11,29(r3)      +0x1C, +0x1D
//      stw  r11,40(r3)  ; sth r11,44(r3)      +0x28, +0x2C
//      stw  r11,48(r3)                        +0x30
//      stw  r11,64(r3) ... stw r11,76(r3)     +0x40, +0x44, +0x48, +0x4C
//      stw  r10,52(r3)        +0x34 = this+52    AGAIN
//      stw  r10,56(r3)        +0x38 = this+52    AGAIN
//      stw  r11,60(r3)        +0x3C = 0          AGAIN
//      blr
//
// Both float constants are read out of the image: 82002D40 is 1.0f and
// 82002DA4 is 0.0f, the same pool word src/n8 and src/n11 use.
//
// THE SENTINEL AT +0x04 IS THE SAME IDIOM AS src/h5_dsp_ctor.cpp AND
// src/n16_dsp_sink_ctor.cpp -- {this+4, this+4, 0}, stored BEFORE the vptr,
// so it belongs to a non-polymorphic base rather than to this class. The
// vtable is 820633D0, not the 8205E640 those two share, so this is a
// different hierarchy that uses the same list type.
//
// THE +0x34 SENTINEL IS WRITTEN TWICE, with all eleven of the body's other
// stores in between and no load anywhere -- so nothing between them can read
// 0x34..0x3F and MSVC's dead-store elimination was entitled to drop the first
// group and did not. That is the shape MATCHED.md and README record as
// genuinely open (sub_82700B30, sub_82583290): a flat constructor loses its
// duplicate stores to DSE, and only a real base subobject has ever kept one
// alive. It is written here as a member sub-object constructed in the
// initialiser list plus an explicit re-initialisation at the end of the body,
// which is the shape the emitted positions describe -- first group
// immediately after the vptr, second group after everything else.

#include "types.h"

struct ListLink;

struct SentinelList
{
    /* 0x00 */ ListLink* head;
    /* 0x04 */ ListLink* tail;
    /* 0x08 */ s32       count;

    SentinelList()
    {
        head  = (ListLink*)this;
        tail  = (ListLink*)this;
        count = 0;
    }
};
ASSERT_SIZE(SentinelList, 0x0C);

struct ListRoot
{
    /* 0x04 */ SentinelList list04;
};

struct ListOwner : public ListRoot
{
    /* 0x00 */                             // vptr -- 820633D0
    /* 0x10 */ char         unk0010[0x04];
    /* 0x14 */ s32          f14;
    /* 0x18 */ s32          f18;
    /* 0x1C */ u8           f1C;
    /* 0x1D */ u8           f1D;
    /* 0x1E */ char         unk001E[0x02];
    /* 0x20 */ f32          f20;
    /* 0x24 */ f32          f24;
    /* 0x28 */ s32          f28;
    /* 0x2C */ u16          f2C;
    /* 0x2E */ char         unk002E[0x02];
    /* 0x30 */ s32          f30;
    /* 0x34 */ SentinelList list34;
    /* 0x40 */ s32          f40;
    /* 0x44 */ s32          f44;
    /* 0x48 */ s32          f48;
    /* 0x4C */ s32          f4C;

    ListOwner();
    virtual void Slot0();
};
ASSERT_OFFSET(ListOwner, list04, 0x04);
ASSERT_OFFSET(ListOwner, f14, 0x14);
ASSERT_OFFSET(ListOwner, f20, 0x20);
ASSERT_OFFSET(ListOwner, f2C, 0x2C);
ASSERT_OFFSET(ListOwner, list34, 0x34);
ASSERT_OFFSET(ListOwner, f4C, 0x4C);

ListOwner::ListOwner()
{
    f20 = 1.0f;
    f24 = 0.0f;

    f14 = 0;
    f18 = 0;
    f1C = 0;
    f1D = 0;
    f28 = 0;
    f2C = 0;
    f30 = 0;
    f40 = 0;
    f44 = 0;
    f48 = 0;
    f4C = 0;

    list34.head  = (ListLink*)&list34;
    list34.tail  = (ListLink*)&list34;
    list34.count = 0;
}
