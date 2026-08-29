#include "types.h"

// sub_82167FE0 -- fetch two optional scale factors through a descriptor and
// multiply a pair of adjacent floats by them. 212 B, 10 callers.  r3 = owner.
//
//   b = a->f30 ; c = b->f54 ; d = c->f1C
//   off0 = d->f64 ; if (off0 == 0xFFFF) return;          <- beqlr, so the
//                                                           guard is first
//   bases[3] = { c->f24, b->f58, a->f14 }   -- a LOCAL array in the red zone
//                                              at -16(r1), indexed by a byte
//   s0 = s1 = 0.0f                          -- lfs 0.0f then `fmr f13,f0`
//
//   off1 = d->f66 ; if (off1 != 0xFFFF) { p = bases[d->f70] + off1;
//                                         if (p) s0 = (*p)->f1C; }
//   if (s0 == 0.0f) { if (b->fA8 & 4) s0 = 1.0f; }
//   off2 = d->f68 ; if (off2 != 0xFFFF) { q = bases[d->f71] + off2;
//                                         if (q) s1 = (*q)->f1C; }
//
//   dst = a->f14 + off0 ; dst[0] *= s0 ; dst[1] *= s1
//
// The `s0 == 0.0f` test is JUMP-THREADED: MSVC emits the fcmpu only on the
// path that actually loaded a value, and both skips of that path branch
// straight into the flag test, because there s0 is still the literal 0.0f.
// So one `if (s0 == 0.0f)` in the source becomes one compare and two extra
// branch targets.
//
// `add. r10,r7,r10 ; beq-` tests the SUM, not the base -- the pointer is
// formed first and then tested.
//
// `rlwinm r9,r10,0,29,29` with a separate `cmplwi` rather than a single
// `rlwinm.` is the /O2 spelling of the bit test; /Os folds it (see the
// sub_827156B8 note in MATCHED.md).
//
// THE OFFSETS ARE READ INLINE, NOT NAMED. Written as `u16 off1 = d->off1;`
// and used twice, this is 41 of 53: the `cmplwi` against 0xFFFF moves ahead
// of the three stores that build the local array, `s->base0` and `r->base2`
// swap issue order, and r3 dies early enough for the allocator to reuse it
// (`lwz r3,88(r9)` where the target has r4). Spelling `d->off1` at both the
// test and the addition is 49 of 49. Naming a 16-bit field forces the
// zero-extended value to be materialised where the two reads would otherwise
// be scheduled independently -- the same family as the /Os `x > 0` versus
// `x != 0` lever, but reached from a load rather than a comparison.
//
// `off0` IS named, and has to be: it is read once and used once, and naming
// it there changes nothing.

struct Node { u8 pad00[0x1C]; f32 value; };
ASSERT_OFFSET(Node, value, 0x1C);

struct Desc
{
    u8  pad00[0x64];
    u16 off0;
    u16 off1;
    u16 off2;
    u8  pad6A[0x06];
    u8  which1;
    u8  which2;
};
ASSERT_OFFSET(Desc, off0, 0x64);
ASSERT_OFFSET(Desc, off1, 0x66);
ASSERT_OFFSET(Desc, off2, 0x68);
ASSERT_OFFSET(Desc, which1, 0x70);
ASSERT_OFFSET(Desc, which2, 0x71);

struct Set
{
    u8    pad00[0x1C];
    Desc* desc;
    u8    pad20[0x04];
    void* base0;
};
ASSERT_OFFSET(Set, desc, 0x1C);
ASSERT_OFFSET(Set, base0, 0x24);

struct Owner
{
    u8    pad00[0x54];
    Set*  set;
    void* base1;
    u8    pad5C[0x4C];
    u8    flags;
};
ASSERT_OFFSET(Owner, set, 0x54);
ASSERT_OFFSET(Owner, base1, 0x58);
ASSERT_OFFSET(Owner, flags, 0xA8);

struct Root
{
    u8     pad00[0x14];
    void*  base2;
    u8     pad18[0x18];
    Owner* owner;
};
ASSERT_OFFSET(Root, base2, 0x14);
ASSERT_OFFSET(Root, owner, 0x30);

void ScalePair(Root* r)
{
    Owner* o = r->owner;
    Set* s = o->set;
    Desc* d = s->desc;

    u16 off0 = d->off0;
    if (off0 == 0xFFFF)
        return;

    void* bases[3];
    bases[0] = s->base0;
    bases[1] = o->base1;
    bases[2] = r->base2;

    float s0 = 0.0f;
    float s1 = 0.0f;

    if (d->off1 != 0xFFFF)
    {
        Node** p = (Node**)((u8*)bases[d->which1] + d->off1);
        if (p)
            s0 = (*p)->value;
    }

    if (s0 == 0.0f)
    {
        if (o->flags & 4)
            s0 = 1.0f;
    }

    if (d->off2 != 0xFFFF)
    {
        Node** q = (Node**)((u8*)bases[d->which2] + d->off2);
        if (q)
            s1 = (*q)->value;
    }

    f32* dst = (f32*)((u8*)r->base2 + off0);
    dst[0] = dst[0] * s0;
    dst[1] = dst[1] * s1;
}
