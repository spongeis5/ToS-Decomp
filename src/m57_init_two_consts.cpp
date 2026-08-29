// sub_82250DB8 -- clear a flag byte and write two float constants.
// 36 bytes, 3 callers.
//
//      lis     r11,-32256
//      lis     r10,-32256
//      li      r9,0
//      stb     r9,40(r3)
//      lfs     f0,11760(r11)       -> 82002DF0, which holds 0.5f
//      lfs     f13,12212(r10)      -> 82002FB4, which holds 0.9999f
//      stfs    f0,0(r3)
//      stfs    f13,4(r3)
//      blr
//
// A separate `lis` per constant again -- two constant-pool symbols, each with
// its own relocated high half. They are 452 bytes apart here, so this one
// could not have shared a base even in principle.
//
// The values were read out of the image: 3F000000 is 0.5f exactly, and
// 3F7FF972 is what `0.9999f` rounds to -- checked by re-encoding, not by
// eyeballing the hex.
//
// Integer and float stores are two streams interleaved by the scheduler; the
// byte is the whole of the integer stream and comes first in it, so the flag
// is written before the two floats.
//
// 4 of 9 words are relocated.

#include "types.h"

struct Fader
{
    /* 0x00 */ f32 from;
    /* 0x04 */ f32 to;
    /* 0x08 */ u8  unk0008[0x20];
    /* 0x28 */ u8  active;
};

ASSERT_OFFSET(Fader, to, 0x04);
ASSERT_OFFSET(Fader, active, 0x28);

void ResetFader(Fader* f)
{
    f->active = 0;
    f->from = 0.5f;
    f->to = 0.9999f;
}
