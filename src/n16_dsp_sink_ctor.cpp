// sub_8257EC60 -- constructor for a second class derived from the SAME base
// as src/h5_dsp_ctor.cpp's DspModule. 128 B, 4 callers.
//
//      lis  r9,-32250 ; lis r8,-32105 ; li r11,0
//      addi r10,r3,4  ; li  r7,-1
//      stw  r11,12(r3)        +0x0C = 0
//      addi r5,r9,-6592       = 8205E640      vtable A
//      stw  r10,4(r3)         +0x04 = this+4
//      lis  r6,-32250
//      stw  r10,8(r3)         +0x08 = this+4
//      stw  r7,16(r3)         +0x10 = -1
//      addi r10,r3,132        this+132
//      stw  r5,0(r3)          vptr A
//      addi r4,r6,2408        = 82060968      vtable B
//      lwz  r9,-14932(r8)     = [8296C5AC]
//      li   r8,1
//      stw  r4,0(r3)          vptr B
//      stw  r9,24(r3)         +0x18 = the global
//      stw  r10,132(r3) ; stw r10,136(r3) ; stw r11,140(r3)
//      stb  r11,44(r3) ; stb r11,45(r3)
//      stw  r11,48(r3) ... stw r11,72(r3)
//      stw  r8,76(r3)         +0x4C = 1
//      blr
//
// THREE ADDRESSES SAY THIS SHARES DspBase, and none of them is a guess:
// vtable A is 8205E640, the same word h5_dsp_ctor stores; the global is
// 8296C5AC, the same one; and the base's own shape is reproduced field for
// field -- a {this+4, this+4, 0} sentinel at +0x04, an id of -1 at +0x10, and
// the global at +0x18. Only the derived vtable differs (82060968 against
// 8205E648), which is exactly what a SIBLING class looks like. So this is the
// second use of a layout that was recovered once, and it is the first
// cross-file type identity in this project supported by more than adjacency.
//
// Everything h5_dsp_ctor established therefore applies unchanged and is not
// re-derived here:
//
//   * BOTH vptr stores to +0x00 survive because the global read is emitted
//     between them, and that only happens if the read is a BASE member
//     initialiser -- a load from a global cannot be disambiguated from
//     *(void**)this, so it makes the first store live. Put the read in the
//     derived class and MSVC deletes the base vptr store as dead.
//   * The four stores AHEAD of the first vptr (0x0C, 0x04, 0x08, 0x10) belong
//     to a base OF the base, because a constructor stores its own vptr before
//     any of its own member initialisers and after its bases have run.
//   * 0x0C landing before 0x04 and 0x08 is the scheduler filling the gap
//     while the `lis`/`addi` for 8205E640 is in flight.
//
// WHAT IS NEW HERE is the derived half, and its order is the ordinary C++ one
// rather than anything chosen: the {this+132, this+132, 0} sentinel at +0x84
// is emitted before every other derived store, which is where a member
// SUB-OBJECT with its own constructor runs -- in the initialiser list, ahead
// of the body. The body then writes 0x2C, 0x2D, 0x30, 0x34, 0x38, 0x3C, 0x40,
// 0x44, 0x48 and 0x4C in address order.
//
// 0x2C and 0x2D are `stb`, so bytes; the rest are `stw` from a zeroed GPR,
// which fixes the width at 4 and says nothing about the type. 0x4C is the
// only non-zero one, at 1.
//
// The three lis/addi pairs and the global load are relocated; the other
// 26 words are compared.

#include "types.h"

struct DspLink;

struct DspList
{
    /* 0x00 */ DspLink* head;
    /* 0x04 */ DspLink* tail;
    /* 0x08 */ s32      count;

    DspList()
    {
        head  = (DspLink*)this;
        tail  = (DspLink*)this;
        count = 0;
    }
};
ASSERT_SIZE(DspList, 0x0C);

struct DspSource;
extern DspSource* g_dsp_source_8296C5AC;

struct DspRoot
{
    /* 0x04 */ DspList list04;
    /* 0x10 */ s32     id10;

    DspRoot() : id10(-1) {}
};

struct DspBase : public DspRoot
{
    /* 0x00 */                          // vptr -- 8205E640
    /* 0x14 */ u32        unk0014;
    /* 0x18 */ DspSource* source18;

    DspBase() : source18(g_dsp_source_8296C5AC) {}
    virtual void Slot0();
};
ASSERT_OFFSET(DspBase, list04, 0x04);
ASSERT_OFFSET(DspBase, id10, 0x10);
ASSERT_OFFSET(DspBase, source18, 0x18);
ASSERT_SIZE(DspBase, 0x1C);

struct DspSink : public DspBase
{
    /* 0x1C */ char    unk001C[0x10];
    /* 0x2C */ u8      f2C;
    /* 0x2D */ u8      f2D;
    /* 0x2E */ char    unk002E[0x02];
    /* 0x30 */ s32     f30;
    /* 0x34 */ s32     f34;
    /* 0x38 */ s32     f38;
    /* 0x3C */ s32     f3C;
    /* 0x40 */ s32     f40;
    /* 0x44 */ s32     f44;
    /* 0x48 */ s32     f48;
    /* 0x4C */ s32     f4C;
    /* 0x50 */ char    unk0050[0x34];
    /* 0x84 */ DspList list84;

    DspSink();
    virtual void Slot0();
    virtual void Slot1();
};
ASSERT_OFFSET(DspSink, f2C, 0x2C);
ASSERT_OFFSET(DspSink, f30, 0x30);
ASSERT_OFFSET(DspSink, f4C, 0x4C);
ASSERT_OFFSET(DspSink, list84, 0x84);

DspSink::DspSink()
{
    f2C = 0;
    f2D = 0;
    f30 = 0;
    f34 = 0;
    f38 = 0;
    f3C = 0;
    f40 = 0;
    f44 = 0;
    f48 = 0;
    f4C = 1;
}
