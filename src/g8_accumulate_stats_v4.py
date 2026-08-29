"""Round 4 for sub_82673020 -- the three accumulator `add`s are not uniform.

    82673060  want add r11,r11,r10   rA = the accumulator   (s12)
    82673070  want add r10,r10,r9    rA = the DELTA         (s16)
    82673080      add r9,r11,r10     rA = the accumulator   (s20) -- already
                                     right with `*p = *p + v`

One `AddTo` cannot produce all three, so this round splits it in two and
tries the four assignments that could give acc/delta/acc.  Everything else is
held at round 3's best (29 of 47): the address-of-member helper for every
accumulator, and a totals helper taking the three delta words as arguments.
"""

H = '#include "types.h"\n\n'

TYPES = """
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

static void AddA(s32* p, s32 v) { *p = v + *p; }
static void AddB(s32* p, s32 v) { *p = *p + v; }

static void AddTotals(Totals* t, s32 a, s32 b, s32 c)
{
    t->t20 = (u16)(t->t20 + a);
    t->t22 = (u16)(t->t22 + b);
    t->t24 = (u16)(t->t24 + c);
}
"""


def body(f12, f16, f20):
    return (H + TYPES + """
void AccumulateStats(Stats* s, Owner* o, const Delta* d)
{
    if (o->totals == 0)
        return;

    s32* pk = &s->peak;
    *pk = Max(*pk, d->d00);
    *pk = Max(*pk, d->d04);

    %s(&s->s12, d->d04);
    %s(&s->s16, d->d08);
    %s(&s->s20, d->d12);

    AddTotals(o->totals, d->d04, d->d08, d->d12);
    *pk = Max(*pk, (s32)o->totals->t20);
}
""" % (f12, f16, f20))


BODIES = [("%s/%s/%s" % (a, b, c), body(a, b, c))
          for a in ("AddA", "AddB")
          for b in ("AddA", "AddB")
          for c in ("AddA", "AddB")]
