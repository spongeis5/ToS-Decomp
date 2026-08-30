#include "types.h"

// sub_821FF818 -- walk four links, return 0 at the first null.
// 64 B, 4 callers.
//
//      lwz     r11,56(r3)   ; beq- -> shared exit
//      lwz     r11,76(r11)  ; beq- -> shared exit
//      lwz     r11,12(r11)  ; beq- -> shared exit
//      lwz     r11,4(r11)   ; beq- -> shared exit
//      lwz     r3,16(r11)
//      blr
//      li      r3,0 ; blr
//
// Every guard branches FORWARD to ONE shared exit -- the || spelling. A
// sequence of separate ifs plants a private li r3,0 after the first test
// (MATCHED.md, the 8219FCD8 lever). Each level loads into the same r11,
// which is what the spelled-out chain compiles to.

struct L4
{
    /* 0x10 */ char  unk0000[16];
    /* 0x10 */ void* f16;
};

struct L3
{
    /* 0x04 */ char unk0000[4];
    /* 0x04 */ L4*  f4;
};

struct L2
{
    /* 0x0C */ char unk0000[12];
    /* 0x0C */ L3*  f12;
};

struct L1
{
    /* 0x4C */ char unk0000[76];
    /* 0x4C */ L2*  f76;
};

struct Root4
{
    /* 0x38 */ char unk0000[56];
    /* 0x38 */ L1*  f56;
};

ASSERT_OFFSET(Root4, f56, 56);
ASSERT_OFFSET(L1, f76, 76);
ASSERT_OFFSET(L2, f12, 12);
ASSERT_OFFSET(L3, f4, 4);
ASSERT_OFFSET(L4, f16, 16);

void* Get4(Root4* r)
{
    if (r->f56 == 0 || r->f56->f76 == 0
        || r->f56->f76->f12 == 0 || r->f56->f76->f12->f4 == 0)
        return 0;
    return r->f56->f76->f12->f4->f16;
}
