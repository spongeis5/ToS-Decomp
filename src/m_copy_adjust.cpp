#include "types.h"

// sub_821A5270 -- read a vector out of a carrier, bias one component, and
// RETURN IT BY VALUE. 48 B, 10 callers.  /O2.
//
//      lwz     r11,120(r4)     c->v.y, as an INTEGER
//      lis     r10,-32256
//      lwz     r9,116(r4)      c->v.x
//      lwz     r8,124(r4)      c->v.z
//      lfs     f0,19944(r10)   ; = 82004DE8 = 0.45f  (3ee66666)
//      stw     r11,4(r3)       r.y  (bit copy into the return buffer)
//      lfs     f13,4(r3)       reload the word just stored, as a float
//      fsubs   f12,f13,f0
//      stfs    f12,4(r3)
//      stw     r9,0(r3)        r.x
//      stw     r8,8(r3)        r.z
//      blr
//
// MATCHED, and the answer is the SIGNATURE, not the body: r3 is not an out
// parameter, it is the HIDDEN RETURN BUFFER of a function whose return type
// is the 12-byte struct.
//
// WHAT THE INTEGER LOADS ACTUALLY MEAN.  This file previously guessed that
// MSVC copies a float through a GPR when no arithmetic is involved, then
// recorded that guess as refuted: with `f32` on both sides of three
// component assignments the compiler emits `lfs f13,120(r4)` and fuses the
// copy straight into the `fsubs`, two instructions short and 0 of 10.  Both
// halves were right and the conclusion drawn from them was too narrow.  The
// `lwz`/`stw` triple is a WHOLE-STRUCT COPY -- `V3 r = c->v;` -- which MSVC
// performs with GPRs whatever the member types are, and the `stw`+`lfs` at
// +4 is that copy's integer store being reloaded as a float, which MSVC
// cannot forward across the type.  So the round trip is evidence of a struct
// assignment, and the struct assignment is the whole function.
//
// Six other shapes were measured against this one, all 48 bytes and all
// wrong, which is what makes the by-value return the answer rather than
// merely a shape that works:
//
//      return by value                     10 of 10   <- this
//      *out = c->v; out->y = out->y - k    3 of 10
//      *out = c->v; out->y -= k            3 of 10
//      V3 t = c->v; *out = t; ...          3 of 10
//      the same as a member function       3 of 10
//      three u32 field copies + float view 3 of 10
//      the adjustment through `f32* py`    1 of 10
//
// The five that keep an explicit `out` parameter all reach 3 of 10 -- they
// do produce the integer copy, so the struct-assignment reading is right --
// but MSVC then schedules the three stores in address order and the biased
// component's chain no longer leads.  With the return buffer it is r3 that
// is written, and the +4 pair is hoisted to the front of the block because
// `stw`->`lfs`->`fsubs`->`stfs` is the critical path.
//
// At /O2 /Os the same source is 8 of 10: the two `fsubs` operands coalesce
// onto one register.  This one wants plain /O2.
struct V3
{
    f32 x;
    f32 y;
    f32 z;
};
ASSERT_SIZE(V3, 12);

struct Carrier
{
    char unk0000[0x74];
    V3   v;
};
ASSERT_OFFSET(Carrier, v, 0x74);

V3 Biased(const Carrier* c)
{
    V3 r = c->v;
    r.y = r.y - 0.45f;
    return r;
}
