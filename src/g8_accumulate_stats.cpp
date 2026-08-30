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
//   82673060, 82673070   the operand order of two of the three `add`s.
//
//     A CORRECTION TO WHAT THIS FILE USED TO SAY HERE. It read: "the target
//     is not uniform -- rA is the ACCUMULATOR at 82673060 and 82673080 and
//     the DELTA at 82673070 -- and no spelling reaches that ... within one
//     retail function the choice is not uniform, so one source cannot produce
//     it." The first half is a measurement and is right. The second half is a
//     conclusion and is wrong, in the way this project keeps recording:
//     OURS IS NOT UNIFORM EITHER. It is non-uniform in the same two places
//     and inverted there:
//
//         82673060  target add r11,r11,r10  rA = acc    ours add r11,r10,r11
//         82673070  target add r10,r10,r9   rA = delta  ours add r10,r11,r10
//         82673080  target add r9,r11,r10   rA = acc    ours agrees
//
//     So a source that produces a mixed pattern is not the thing that is
//     missing -- the current source already does. What is missing is a
//     control over WHICH mixture, and the read-order rule does not supply one
//     here: five helper flavours (`*p = *p + v`, `*p = v + *p`, a named
//     `s32 a = *p` first, and both with the delta passed by POINTER so its
//     read moves inside the helper after `*p`) applied to each accumulator
//     separately and in all 25 pairs -- 29 shapes -- are byte-identical at
//     31 of 47 except one at 29. MSVC canonicalises every one of them.
//     Group 2 also loads its accumulator into r11 where the target uses r9.
//   82673088..826730C4   the totals block, thirteen words, all of them
//     schedule.  Ours loads d08, d04, d12 where the target loads d12, d04,
//     d08, reads t22 before t24, and stores 22 before 20 where the target
//     stores 20 first.  Every load is on the correct side of every store in
//     both.  Six shapes of that block were measured -- three delta arguments,
//     the same with the three `lhz` hoisted into locals, a `Delta*` with the
//     three words read into locals in the target's order, the block inlined
//     with a named `Totals*` local, and both orders of the parameter list --
//     scoring 29, 29, 29, 26, 29 and 29.
//
// NOW 31 OF 47, from the ADDRESS-OF-MEMBER PIN on the first totals field:
//
//     u16* p = &t->t20;
//     *p = (u16)(*p + a);
//
// Two constant offsets off one base provably cannot alias, so MSVC is free
// to issue the +22 store before the +20 store; taking the first field's
// address removes that proof and the store order becomes the source's
// ascending 20, 22, 24, which is the target's. That lever matched three
// other functions in this batch outright (sub_827007F8, sub_825FE880,
// sub_82784F90) and it is worth two words here.
//
// It does not finish the block, and WHERE IT STOPS IS RECORDED: the target
// reads t24, t22, t20 -- descending -- with all three loads ahead of every
// store, while the pinned version issues t20's store as soon as its add is
// ready. Six more shapes were measured against that: pinning `&t24` instead
// (28), pinning both ends (28), routing all three fields through one `u16*`
// (26), the pin plus all three fields read into locals in descending order
// first (196 bytes, 1 of 47 -- the locals cost a word each), the same with
// `&t22` pinned (196 bytes, 1 of 47), the three sums named before any store
// (31, the same as the plain pin), and `+=` compound assignment (29). The
// deltas named as locals in the target's load order before the call is 24,
// which is worse than passing them as arguments.
//
// THE PIN AND THE TARGET'S SHAPE ARE IN OPPOSITION, which is the thing to
// know before spending another session here. A 120-shape sweep -- the six
// statement orders crossed with four pin choices (none, &t20, &t24, all
// three) and four prefetch choices (none, and the three fields read into
// locals ascending, descending, or 24/20/22 first) -- says:
//
//   * WITHOUT any pin, MSVC does put every `lhz` ahead of every `sth`, which
//     is the target's shape, and then chooses the store order itself: three
//     different source orders all emit 20, 24, 22. Best 30 of 47.
//   * WITH the &t20 pin the store order is the source's ascending 20, 22, 24,
//     and the +20 store is planted between the loads. Best 31 of 47.
//
// The target has both, and nothing in the sweep has both. Pinning is what
// removes MSVC's proof that the +22 and +24 loads cannot alias the +20 store,
// so it necessarily forbids hoisting them -- the two goals are opposed along
// this axis, in the same way `count = 0` and `str = string` are opposed in
// sub_8215A5C8. Also measured and no better: the deltas as a `Delta*` the
// block reads itself (26 with or without the pin, 31 with the three words
// read into locals in the target's order), the argument list reordered to the
// target's load order d12/d04/d08 (27) and to d08/d04/d12 (31), the totals
// pointer as an `Owner*` the helper dereferences (26), `o->totals` spelled at
// each use (196 bytes, 23), a two-level `Bump(&t->tNN, v)` helper (28), and
// the three sums named before any store with and without the pin (31, 29).
//
// FLAGS ARE DONE: all 72 combinations from tools/flagsweep.py, 44 of them at
// this same 31 of 47 and 28 at 23 of 47, every one 188 bytes.
//
// About 160 shapes have now been measured on this function in total.

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
    u16* p = &t->t20;
    *p = (u16)(*p + a);
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
