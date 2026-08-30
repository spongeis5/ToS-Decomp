#include "types.h"

// sub_82543D08 -- initialise an object: a vtable and eight zero fields.
// 52 B, 4 callers.
//
//      lis     r10,-32250
//      li      r11,0
//      addi    r9,r10,-7252     = 8205E3AC, this class's vtable
//      stw     r11,264(r3)      <- emitted order is the source order
//      stw     r9,0(r3)
//      stb     r11,268(r3)
//      stw     r11,276(r3)
//      stw     r11,272(r3)
//      stw     r11,280(r3)
//      stw     r11,284(r3)
//      stw     r11,292(r3)      <- 292 before 288
//      stw     r11,288(r3)
//      blr

struct VTable;
extern const VTable kVTable_8205E3AC;

struct Wide52
{
    /* 0x000 */ const VTable* vt;
    /* 0x004 */ char          unk0004[260];
    /* 0x108 */ s32           f264;
    /* 0x10C */ u8            f268;
    /* 0x10D */ char          unk010D[3];
    /* 0x110 */ s32           f272;
    /* 0x114 */ s32           f276;
    /* 0x118 */ s32           f280;
    /* 0x11C */ s32           f284;
    /* 0x120 */ s32           f288;
    /* 0x124 */ s32           f292;
};

ASSERT_OFFSET(Wide52, f264, 264);
ASSERT_OFFSET(Wide52, f268, 268);
ASSERT_OFFSET(Wide52, f288, 288);
ASSERT_OFFSET(Wide52, f292, 292);

void InitWide52(Wide52* s)
{
    s->f264 = 0;
    s->vt   = &kVTable_8205E3AC;
    s->f268 = 0;
    s->f276 = 0;
    s->f272 = 0;
    s->f280 = 0;
    s->f284 = 0;
    s->f292 = 0;
    s->f288 = 0;
}
