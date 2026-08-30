#include "types.h"

// sub_825ACA90 -- initialise an object with two float constants, a data
// pointer, and four zero fields. 68 B, 4 callers.
//
//      lis     r10,-32256
//      lis     r9,-32255
//      lis     r8,-32250
//      li      r11,0
//      addi    r7,r8,3176       = 82060C68, a .rdata address stored whole
//      li      r6,-1
//      lfs     f0,13364(r10)    100.0     82003434
//      lfs     f13,-4704(r9)    1000000.0 8200EDA0
//      stw     r11,24(r3)       <- emitted order is the source order
//      stfs    f0,72(r3)
//      stw     r7,0(r3)
//      stfs    f13,68(r3)
//      stw     r11,4(r3)
//      stw     r11,20(r3)
//      stw     r11,12(r3)
//      stw     r6,52(r3)
//      blr

struct VTable;
extern const VTable kVTable_82060C68;

struct SimA
{
    /* 0x00 */ const VTable* vt;
    /* 0x04 */ s32   f4;
    /* 0x08 */ char  unk0008[4];
    /* 0x0C */ s32   f12;
    /* 0x10 */ char  unk0010[4];
    /* 0x14 */ s32   f20;
    /* 0x18 */ s32   f24;
    /* 0x1C */ char  unk001C[24];
    /* 0x34 */ s32   f52;
    /* 0x38 */ char  unk0038[12];
    /* 0x44 */ float f68;
    /* 0x48 */ float f72;
};

ASSERT_OFFSET(SimA, f12, 12);
ASSERT_OFFSET(SimA, f52, 52);
ASSERT_OFFSET(SimA, f68, 68);
ASSERT_OFFSET(SimA, f72, 72);

void InitSimA(SimA* s)
{
    s->f24  = 0;
    s->f72  = 100.0f;
    s->vt   = &kVTable_82060C68;
    s->f68  = 1000000.0f;
    s->f4   = 0;
    s->f20  = 0;
    s->f12  = 0;
    s->f52  = -1;
}
