// sub_82160880 -- 36 bytes, 100 callers. Copies four fields of a source
// record into a sub-object at +0x18 of the destination and zeroes the last.
//
//      ld      r11,0(r4)       ; 64-bit
//      li      r10,0
//      std     r11,24(r3)
//      lwz     r9,8(r4)
//      stw     r9,32(r3)
//      lhz     r8,14(r4)
//      sth     r8,38(r3)
//      stw     r10,40(r3)
//      blr
//
// Read off the listing:
//   * ld/std, not two lwz -- the first field is genuinely 8 bytes wide.
//   * lhz/sth -- the third is an unsigned 16-bit field, at +14 not +12, so
//     +12 is something else that is NOT copied.
//   * Every destination offset is its source offset plus 24, so the writes
//     are into a member sub-object at 0x18 rather than into scattered
//     fields; the trailing stw of zero lands at that sub-object's +16.
//   * `li r10,0` is hoisted to the top by the scheduler, but the store it
//     feeds is emitted LAST, so the zero is the last statement in source.
//
// Store order is source order: 0x18, 0x20, 0x26, 0x28.

#include "types.h"

struct Rec
{
    /* 0x00 */ u64 a;
    /* 0x08 */ u32 b;
    /* 0x0C */ u16 unk000C;
    /* 0x0E */ u16 c;
    /* 0x10 */ u32 d;
};

ASSERT_OFFSET(Rec, a, 0x00);
ASSERT_OFFSET(Rec, b, 0x08);
ASSERT_OFFSET(Rec, c, 0x0E);
ASSERT_OFFSET(Rec, d, 0x10);

struct Owner
{
    /* 0x00 */ char unk0000[0x18];
    /* 0x18 */ Rec  rec;
};

ASSERT_OFFSET(Owner, rec, 0x18);

void SetRecord(Owner* o, const Rec* s)
{
    o->rec.a = s->a;
    o->rec.b = s->b;
    o->rec.c = s->c;
    o->rec.d = 0;
}
