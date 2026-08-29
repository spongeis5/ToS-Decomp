// sub_821F61B8 -- pick one of four codes from a flag and a mode.
// 60 bytes, 3 callers.
//
//      lbz   r11,132(r3) ; cmplwi cr6,r11,0
//      lwz   r11,224(r3)                      hoisted ABOVE the branch
//      beq-  cr6,<off>
//      cmpwi cr6,r11,1 ; bne- cr6,<a22>
//      li    r3,20 ; blr
//  a22: li   r3,22 ; blr
//  off:  cmpwi cr6,r11,1
//      li    r3,21
//      beqlr cr6
//      li    r3,31 ; blr
//
// The mode is loaded ONCE, before the first branch, and both arms use it --
// so it is one read in the source and MSVC hoisted it, not two reads that
// happened to CSE. Nothing stores in between, which is what makes the hoist
// legal.
//
// `beq-` jumping away to the off-arm means the flag-set path is the
// fall-through and is written first.
//
// In the second arm `li r3,21` is emitted BEFORE `beqlr`, which is the
// materialisation a conditional return needs -- the value has to be in r3
// already. It is not the accumulator shape from sub_82806FD0: that one puts
// the DEFAULT above the guard and assigns the interesting value inside,
// whereas here 21 is the value the guard returns.
//
// `lbz`+`cmplwi` on the flag is an unsigned byte; `cmpwi` on the mode is a
// signed int.
//
// Nothing is relocated: 15 of 15 words are compared.

#include "types.h"

struct Widget
{
    /* 0x00 */ u8  unk0000[0x84];
    /* 0x84 */ u8  enabled;
    /* 0x85 */ u8  unk0085[0x5B];
    /* 0xE0 */ s32 mode;
};

ASSERT_OFFSET(Widget, enabled, 132);
ASSERT_OFFSET(Widget, mode, 224);

int WidgetCode(Widget* w)
{
    if (w->enabled)
    {
        if (w->mode == 1)
            return 20;
        return 22;
    }

    if (w->mode == 1)
        return 21;
    return 31;
}
