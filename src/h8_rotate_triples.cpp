#include "types.h"

// sub_82836078 -- emit two runs of up to three 16-bit indices, each run
// rotated so its largest value comes first. 324 B, 5 callers.
//
// IT IS A ROTATION, NOT A SORT, and that is worth stating because the code
// reads like a sort. Two conditional swaps against the running maximum, a
// COUNT of how many fired, and a third swap of the two survivors when the
// count is exactly one. Enumerating the three outcomes:
//
//      a >= b, a >= c        ->  a b c
//      b >  a, b >= c        ->  b c a
//      c >  a                ->  c a b
//
// which are the three cyclic rotations of (a,b,c), never (a,c,b). Cyclic
// order is preserved, so this is canonicalising a triple whose ORIENTATION
// matters -- a winding order -- not ordering it.
//
// EVERY RELOAD IS EXPLAINED BY ALIASING, which is why no field is hoisted
// into a local here. `s->n0` is loaded at 82836078 and again ten bytes later
// at 82836088 with nothing but a `stb` through `o` in between: the compiler
// cannot prove `o != s`, so the store kills the cached load. The two guard
// compares at 82836094 and 828360C4 share one register because only loads
// separate them. And the three reloads at the bottom are each preceded by an
// `sth` through `o`. Spelling the field out at every use is what produces
// exactly this pattern; naming it in a local would produce one load.
//
// The guards are `blt` past everything and `ble` past the last store, so the
// run length gates how many of the three are written:
//   n < 2  ->  idx[0] only
//   n == 2 ->  idx[0], idx[1]
//   n > 2  ->  all three
//
// The second run's stores are indexed `s->n0 + k`, so it is appended after
// the first run rather than written at a fixed place.
//
// The values are 32-bit fields truncated to 16 bits -- `lwz` then
// `clrlwi ...,16`, not `lhz` -- and held in `u16` locals, which is what the
// redundant second `clrlwi` before each `cmplw` says: MSVC re-normalises a
// short local at the comparison even when the register is already clean,
// while a plain `mr` copy of one carries no mask.

struct TriSrc
{
    /* 0x000 */ s32  n0;
    /* 0x004 */ s32  n1;
    /* 0x008 */ char unk0008[0x24];
    /* 0x02C */ u32  a0;
    /* 0x030 */ char unk0030[0x0C];
    /* 0x03C */ u32  a1;
    /* 0x040 */ char unk0040[0x0C];
    /* 0x04C */ u32  a2;
    /* 0x050 */ char unk0050[0x9C];
    /* 0x0EC */ u32  b0;
    /* 0x0F0 */ char unk00F0[0x0C];
    /* 0x0FC */ u32  b1;
    /* 0x100 */ char unk0100[0x0C];
    /* 0x10C */ u32  b2;
};
ASSERT_OFFSET(TriSrc, n0, 0x000);
ASSERT_OFFSET(TriSrc, n1, 0x004);
ASSERT_OFFSET(TriSrc, a0, 0x02C);
ASSERT_OFFSET(TriSrc, a1, 0x03C);
ASSERT_OFFSET(TriSrc, a2, 0x04C);
ASSERT_OFFSET(TriSrc, b0, 0x0EC);
ASSERT_OFFSET(TriSrc, b1, 0x0FC);
ASSERT_OFFSET(TriSrc, b2, 0x10C);

struct TriOut
{
    /* 0x00 */ u16 idx[4];
    /* 0x08 */ u8  c0;
    /* 0x09 */ u8  c1;
};
ASSERT_OFFSET(TriOut, idx, 0x00);
ASSERT_OFFSET(TriOut, c0, 0x08);
ASSERT_OFFSET(TriOut, c1, 0x09);

void BuildTriples(TriSrc* s, TriOut* o)
{
    o->c0 = (u8)s->n0;
    o->c1 = (u8)s->n1;

    u16 x = (u16)s->a0;
    if (s->n0 >= 2)
    {
        u16 y = (u16)s->a1;
        s32 swaps = 0;
        if (y > x)
        {
            u16 t = y;
            y = x;
            x = t;
            swaps++;
        }
        if (s->n0 > 2)
        {
            u16 z = (u16)s->a2;
            if (z > x)
            {
                u16 t = z;
                z = x;
                x = t;
                swaps++;
            }
            if (swaps == 1)
            {
                u16 t = z;
                z = y;
                y = t;
            }
            o->idx[2] = z;
        }
        o->idx[1] = y;
    }
    o->idx[0] = x;

    u16 p = (u16)s->b0;
    if (s->n1 >= 2)
    {
        u16 q = (u16)s->b1;
        s32 swaps = 0;
        if (q > p)
        {
            u16 t = q;
            q = p;
            p = t;
            swaps++;
        }
        if (s->n1 > 2)
        {
            u16 r = (u16)s->b2;
            if (r > p)
            {
                u16 t = r;
                r = p;
                p = t;
                swaps++;
            }
            if (swaps == 1)
            {
                u16 t = r;
                r = q;
                q = t;
            }
            o->idx[s->n0 + 2] = r;
        }
        o->idx[s->n0 + 1] = q;
    }
    o->idx[s->n0] = p;
}
