// sub_82673020 -- fold a delta record into a running stats block and into the
// owner's 16-bit totals, keeping a high-water mark. 188 B, 5 callers.
// r3 = stats, r4 = owner (totals pointer at +48), r5 = the delta.
//
//      lwz  r11,48(r4) ; cmplwi ; beqlr        guard as a conditional return
//      max into 8(r3) TWICE, with a store between them -- the store cannot
//        be eliminated because the following load is through a DIFFERENT
//        pointer that MSVC cannot prove does not alias it
//      12(r3) += 4(r5) ; 16(r3) += 8(r5) ; 20(r3) += 12(r5)
//      ONE lwz 48(r4), the three delta words and the three `lhz` ALL loaded
//        before any `sth`, then three adds, then three `sth`
//      a third lwz 48(r4) and a final max against (s32)totals->t20
//
// Two things the listing states and a plain spelling gets wrong.  Each
// `lwz <next accumulator>(r3)` comes out one instruction ABOVE the previous
// `stw` -- two constant offsets off one base provably cannot alias, so MSVC
// moves the load up -- and MATCHED.md's sub_827FEE48 lever, a pointer TO the
// member stored through, is what stops it; that is the whole of the `AddTo`
// helper here.  And the totals pointer is loaded ONCE for the three 16-bit
// updates, so that block takes the pointer as a value rather than spelling
// `o->totals` at each `sth`, which would force two reloads.
//
// The three delta words being loaded ahead of every `sth` is what passing
// them as ARGUMENTS gives: written as `t->tNN += d->dNN` inside the helper,
// each load sits below the previous store and cannot be hoisted past it.
//
// NEAR MISS: 29 of 47 words, at the exact size of 188 bytes.  Correct from
// 82673020 through 8267305C -- the guard, both maxima, the store between
// them and the first accumulator's two loads.  What differs:
//
//   82673060, 82673070   the operand order of two of the three `add`s.  The
//     target is not uniform -- rA is the ACCUMULATOR at 82673060 and 82673080
//     and the DELTA at 82673070 -- and no spelling reaches that.  All EIGHT
//     assignments of `*p = *p + v` against `*p = v + *p` across the three
//     accumulators were compiled, and all eight score 29 of 47 with this
//     same diff; so were all eight orders written as explicit member
//     pointers without the helper, and those score 27.  This is the
//     counter-example class from MATCHED.md's `lwzx` note: within one retail
//     function the choice is not uniform, so one source cannot produce it.
//   82673088..826730C4   the totals block, thirteen words, all of them
//     schedule.  Ours loads d08, d04, d12 where the target loads d12, d04,
//     d08, reads t22 before t24, and stores 22 before 20 where the target
//     stores 20 first.  Every load is on the correct side of every store in
//     both.  Six shapes of that block were measured -- three delta arguments,
//     the same with the three `lhz` hoisted into locals, a `Delta*` with the
//     three words read into locals in the target's order, the block inlined
//     with a named `Totals*` local, and both orders of the parameter list --
//     scoring 29, 29, 29, 26, 29 and 29.

#include "types.h"

struct Totals
{
    /* 0x00 */ char unk0000[20];
    /* 0x14 */ u16  t20;
    /* 0x16 */ u16  t22;
    /* 0x18 */ u16  t24;
};
ASSERT_OFFSET(Totals, t20, 20);
ASSERT_OFFSET(Totals, t24, 24);

struct Owner
{
    /* 0x00 */ char    unk0000[48];
    /* 0x30 */ Totals* totals;
};
ASSERT_OFFSET(Owner, totals, 48);

struct Delta
{
    /* 0x00 */ s32 d00;
    /* 0x04 */ s32 d04;
    /* 0x08 */ s32 d08;
    /* 0x0C */ s32 d12;
};

struct Stats
{
    /* 0x00 */ char unk0000[8];
    /* 0x08 */ s32  peak;
    /* 0x0C */ s32  s12;
    /* 0x10 */ s32  s16;
    /* 0x14 */ s32  s20;
};
ASSERT_OFFSET(Stats, peak, 8);
ASSERT_OFFSET(Stats, s20, 20);

static s32 Max(s32 x, s32 y)
{
    return x > y ? x : y;
}

static void AddTo(s32* p, s32 v)
{
    *p = *p + v;
}

static void AddTotals(Totals* t, s32 a, s32 b, s32 c)
{
    t->t20 = (u16)(t->t20 + a);
    t->t22 = (u16)(t->t22 + b);
    t->t24 = (u16)(t->t24 + c);
}

void AccumulateStats(Stats* s, Owner* o, const Delta* d)
{
    if (o->totals == 0)
        return;

    s32* pk = &s->peak;
    *pk = Max(*pk, d->d00);
    *pk = Max(*pk, d->d04);

    AddTo(&s->s12, d->d04);
    AddTo(&s->s16, d->d08);
    AddTo(&s->s20, d->d12);

    AddTotals(o->totals, d->d04, d->d08, d->d12);

    *pk = Max(*pk, (s32)o->totals->t20);
}
