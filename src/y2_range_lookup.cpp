// sub_825FC978 -- find which of four ranges holds a value and return that
// range's id, -1 when the value is zero, or 0 when no range holds it.
// 240 bytes, 2 callers.
//
// The same eleven-instruction block appears four times over the pointers at
// +32, +36, +40 and +44:
//
//      lwz    r11,32(r3)         the range object
//      lwz    r10,48(r11)        lo
//      cmplw  cr6,r4,r10
//      blt-   cr6,false
//      lwz    r10,52(r11)        hi
//      cmplw  cr6,r4,r10
//      li     r10,1
//      blt-   cr6,done
// false:li    r10,0
// done: clrlwi r10,r10,24        <- the BOOL normalisation
//      cmplwi cr6,r10,0
//      beq-   cr6,<next block>
//
// A 0/1 VALUE is built and then masked, which per MATCHED.md is an inlined
// `bool`-returning helper rather than a bare `if (a && b)` -- a plain `&&`
// branches out of each term and never materialises anything. The short
// circuit is visible in the value form too: the first `blt-` skips straight
// to `li r10,0` without touching `hi`.
//
// The `cmplw` on both bounds says the value and the bounds are UNSIGNED.
//
// The three later blocks' hit paths branch BACKWARD into 825FC9EC, the
// second block's own tail, because blocks 2, 3 and 4 all keep the range
// object in r10 and their tails are identical, so MSVC merged them. Block 1
// keeps it in r11 and so keeps its own copy -- which is why the same source
// statement appears twice in the image and not four times.
//
// WHICH ARM OF THE `if`/`else` IS WRITTEN FIRST DECIDES WHERE `li r3,-1` IS
// EMITTED, and that one word was the whole function. The tail wanted is
//
//      cmplwi cr6,r4,0 ; li r3,-1 ; beqlr cr6 ; lwz r3,44(r11) ; blr
//
// -- the failure value materialised BETWEEN the compare and its branch.
// Five spellings were measured at /O2 against the 60-word target:
//
//   if (v == 0) return -1; return r->id;          38 of 60 -- `bne-` over a
//                                                   private `li ; blr`
//   s32 id = -1; if (v != 0) id = r->id;          56 of 60 -- right structure,
//                                                   but the initialiser puts
//                                                   `li` BEFORE the compare
//   return v != 0 ? r->id : -1;                   38 of 60 -- and it merges
//                                                   all four tails into one
//   if (v != 0) id = r->id; else id = -1;         38 of 60 -- same merge
//   if (v == 0) id = -1; else id = r->id;         60 of 60
//
// `/O2 /Os` on the 56-of-60 form is 11 of 56 with a size difference, so this
// is not the optimisation level.
//
// The rule the last two lines carry is that an `if`/`else` is NOT symmetric
// to MSVC here: the arm written first is the one whose constant is hoisted
// into the compare's delay slot, and swapping the arms moves it. That is a
// companion to MATCHED.md's note on sub_825BFFF0, where no `if`/`else`
// spelling could place the materialisation and a `goto` was needed -- here
// the polarity of the `if` is enough, and it is worth trying before the
// heavier shapes.

#include "types.h"

struct Range
{
    /* 0x00 */ u8  unk0000[0x2C];
    /* 0x2C */ s32 id;
    /* 0x30 */ u32 lo;
    /* 0x34 */ u32 hi;
};
ASSERT_OFFSET(Range, id, 0x2C);
ASSERT_OFFSET(Range, lo, 0x30);
ASSERT_OFFSET(Range, hi, 0x34);

struct RangeSet
{
    /* 0x00 */ u8     unk0000[0x20];
    /* 0x20 */ Range* a;
    /* 0x24 */ Range* b;
    /* 0x28 */ Range* c;
    /* 0x2C */ Range* d;
};
ASSERT_OFFSET(RangeSet, a, 0x20);
ASSERT_OFFSET(RangeSet, b, 0x24);
ASSERT_OFFSET(RangeSet, c, 0x28);
ASSERT_OFFSET(RangeSet, d, 0x2C);

static bool Holds(const Range* r, u32 v)
{
    return v >= r->lo && v < r->hi;
}

s32 RangeIdOf(RangeSet* s, u32 v)
{
    if (Holds(s->a, v))
    {
        s32 id;
        if (v == 0)
            id = -1;
        else
            id = s->a->id;
        return id;
    }
    if (Holds(s->b, v))
    {
        s32 id;
        if (v == 0)
            id = -1;
        else
            id = s->b->id;
        return id;
    }
    if (Holds(s->c, v))
    {
        s32 id;
        if (v == 0)
            id = -1;
        else
            id = s->c->id;
        return id;
    }
    if (Holds(s->d, v))
    {
        s32 id;
        if (v == 0)
            id = -1;
        else
            id = s->d->id;
        return id;
    }
    return 0;
}
