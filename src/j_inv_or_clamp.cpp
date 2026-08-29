#include "types.h"

// sub_821AC2F0 -- reciprocal of a field, with a clamp for negatives and a
// zero passthrough. 72 B, 11 callers.  r3 = source object, r4 = destination.
//
//   lfs f0,32(r3) ; lfs f13,11684(r11)   -> 82002DA4 = 0.0f
//   fcmpu cr6,f0,f13 ; bge- ...          v < 0 falls through
//     lfs f0,20564(r11)                  -> 82005054 = 6.6666665f
//     stfs f0,40(r4) ; blr
//   fcmpu cr6,f0,f13 ; bne- ...          v == 0 falls through
//     stfs f13,40(r4) ; blr              the SAME 0.0f register
//   lfs f13,11584(r11)                   -> 82002D40 = 1.0f
//   fdivs f0,f13,f0 ; stfs f0,40(r4)
//
// The 0.0f is compared TWICE against the same value in the same register --
// two separate `if`s, each with its own fcmpu; MSVC does not reuse cr6 here.
// Both guards are the negated jump-away form, so both bodies are written as
// the fall-through and the reciprocal is last.

struct Src { u8 pad00[0x20]; f32 v; };
ASSERT_OFFSET(Src, v, 0x20);

struct Dst { u8 pad00[0x28]; f32 out; };
ASSERT_OFFSET(Dst, out, 0x28);

void InvOrClamp(const Src* s, Dst* d)
{
    float v = s->v;

    if (v < 0.0f)
    {
        d->out = 6.6666665f;
        return;
    }

    if (v == 0.0f)
    {
        d->out = 0.0f;
        return;
    }

    d->out = 1.0f / v;
}
