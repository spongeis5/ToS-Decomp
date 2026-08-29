#include "types.h"

// sub_821A5270 -- copy a vector and bias one component. 48 B, 10 callers.
//
//      lwz     r11,120(r4)     b->v.y, as an INTEGER
//      lis     r10,-32256
//      lwz     r9,116(r4)      b->v.x
//      lwz     r8,124(r4)      b->v.z
//      lfs     f0,19944(r10)   ; = 82004DE8 = 0.45f
//      stw     r11,4(r3)       out->y  (bit copy)
//      lfs     f13,4(r3)       reload the word just stored, as a float
//      fsubs   f12,f13,f0
//      stfs    f12,4(r3)
//      stw     r9,0(r3)        out->x
//      stw     r8,8(r3)        out->z
//      blr
//
// NOT MATCHED -- 0 of 10, and the hypothesis in this comment was WRONG, so
// it is kept as written rather than quietly replaced.
//
// The guess was that MSVC copies a float through a GPR when no arithmetic is
// involved, and that the stw/lfs round trip at +4 was it failing to forward
// its own store. It does not: with `f32` on both sides the compiler emits
// `lfs f13,120(r4)` and fuses the copy straight into the `fsubs`, giving 10
// wrong words and a body two instructions short.
//
// So the integer loads are a fact about the TYPES, not about the scheduler:
// something on one side of this copy is not a float. The next thing to try
// is a source whose fields are u32 with the y arithmetic reached through a
// float view, which is what a bit copy plus one float adjustment looks like
// when it is written honestly.
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

void CopyBiased(V3* out, const Carrier* c)
{
    out->y = c->v.y;
    out->y = out->y - 0.45f;
    out->x = c->v.x;
    out->z = c->v.z;
}
