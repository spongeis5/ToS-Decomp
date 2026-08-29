#include "types.h"

// sub_821BCA48 -- constructor, vtable and seven zeroed words. 44 B.
//
//      lis     r10,-32256
//      li      r11,0
//      addi    r9,r10,29668     ; = 820073E4, a vtable
//      stw     r11,4(r3)        +4 FIRST
//      stw     r9,0(r3)         then the vtable
//      stw     r11,8(r3)  12  20  24  28
//      blr
//
// +0x10 is SKIPPED, so it is not a run of zeroes -- seven named fields with
// one left alone.
//
// The vtable store lands SECOND even though it is written first in source:
// its address is still in flight through the lis/addi pair, and the
// scheduler fills the gap with the first independent store. That is the same
// behaviour m_ctor_94.cpp runs into -- there the target wants the vtable
// FIRST and no source order produces it, which is still unmatched.
struct VT821BCA48;
extern const VT821BCA48 kVTable_820073E4;

struct Tracker
{
    /* 0x00 */ const VT821BCA48* vt;
    /* 0x04 */ s32  f04;
    /* 0x08 */ s32  f08;
    /* 0x0C */ s32  f0C;
    /* 0x10 */ s32  unk0010;
    /* 0x14 */ s32  f14;
    /* 0x18 */ s32  f18;
    /* 0x1C */ s32  f1C;
};
ASSERT_OFFSET(Tracker, f1C, 0x1C);

void ConstructTracker(Tracker* t)
{
    t->vt  = &kVTable_820073E4;
    t->f04 = 0;
    t->f08 = 0;
    t->f0C = 0;
    t->f14 = 0;
    t->f18 = 0;
    t->f1C = 0;
}
