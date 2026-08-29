// sub_821A4318 -- store five floats and set one flag bit from a seventh
// argument. 36 bytes, 3 callers.
//
//      lbz    r11,116(r3)
//      stfs   f1,92(r3)
//      stfs   f2,96(r3)
//      rlwimi r11,r9,7,17,24
//      stfs   f3,104(r3)
//      stfs   f4,108(r3)
//      stfs   f5,112(r3)
//      stb    r11,116(r3)
//      blr
//
// **r9 is the tell, and it fixes the signature.** Nothing in the function
// writes r9, so it is an incoming argument -- and it is the SEVENTH GPR slot.
// A float argument consumes its GPR slot without using it on this ABI, so
// `this` takes r3, the five floats take f1..f5 and the r4..r8 slots with
// them, and the next integer parameter lands in r9. Six parameters would put
// it in r4.
//
// `rlwimi r11,r9,7,17,24` shifts the argument left seven and keeps eight bits
// of it, at word positions 7..14. Only ONE of those -- position 7 -- survives
// the `stb`, so the flag written is 0x80 of the byte, and the eight-bit mask
// is MSVC declining to narrow what the store narrows anyway. That the mask is
// eight bits wide rather than one is what says the shifted value is a BYTE
// and not a single bit.
//
// The float store at +100 is missing from a run of 92, 96, 104, 108, 112, so
// there is a member between them this function does not touch.
//
// Integer and float stores are two streams: the lone integer store is last in
// its own stream and the loaded byte is hoisted to the top, so the flag
// statement is written after the five assignments.
//
// Nothing is relocated: 9 of 9 words are compared.

#include "types.h"

struct Emitter
{
    /* 0x00 */ u8  unk0000[0x5C];
    /* 0x5C */ f32 a;
    /* 0x60 */ f32 b;
    /* 0x64 */ f32 unk0064;
    /* 0x68 */ f32 c;
    /* 0x6C */ f32 d;
    /* 0x70 */ f32 e;
    /* 0x74 */ u8  flags;
};

ASSERT_OFFSET(Emitter, a, 0x5C);
ASSERT_OFFSET(Emitter, e, 0x70);
ASSERT_OFFSET(Emitter, flags, 0x74);

void SetShape(Emitter* em, float a, float b, float c, float d, float e, u8 on)
{
    em->a = a;
    em->b = b;
    em->c = c;
    em->d = d;
    em->e = e;
    em->flags = (u8)((em->flags & 0x7F) | (on << 7));
}
