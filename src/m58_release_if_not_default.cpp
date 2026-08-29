// sub_82706950 -- release a handle unless it is the shared default or null.
// 36 bytes, 3 callers.
//
//      mr      r11,r3
//      lwz     r3,0(r3)
//      lwz     r11,8(r11)
//      cmplw   cr6,r3,r11
//      beqlr   cr6
//      cmplwi  cr6,r3,0
//      beqlr   cr6
//      b       0x82662E08          -> ReleaseHandle, the image's most-called
//      blr                          <- unreachable, and counted in the size
//
// The handle is loaded STRAIGHT INTO r3, the argument register, and the
// object pointer is what gets copied out of the way into r11. Per the
// un-naming lever on sub_8224E178 that is the NAMED-local form: spelling
// `o->handle` at each of its three uses would CSE it into a scratch and add
// a `rlwinm`/`mr` to materialise r3.
//
// Two `beqlr` -- a conditional RETURN, not a branch to a shared block -- so
// the guard-direction lever from sub_821675B8 does not apply here and either
// spelling of the two tests gives the same instruction.
//
// The trailing `blr` is the dead one after a tail call, and the recorded size
// includes it.
//
// `/O2 /Os`, and the signature is the textbook one: at /O2 the instructions,
// their order and the length are all already right and the second load takes
// a FRESH `lwz r10,8(r11)` where retail reuses its own base, `lwz r11,8(r11)`
// -- two words, both register names, nothing a source shape can reach.
//
// The tail branch is relocated, so 8 of 9 words are compared.

#include "types.h"

void ReleaseHandle(u32 h);

struct Slot70
{
    /* 0x00 */ u32 handle;
    /* 0x04 */ u8  unk0004[4];
    /* 0x08 */ u32 shared;
};

ASSERT_OFFSET(Slot70, shared, 0x08);

void ReleaseIfOwned(Slot70* s)
{
    u32 h = s->handle;

    if (h != s->shared && h != 0)
        ReleaseHandle(h);
}
