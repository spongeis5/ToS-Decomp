#include "types.h"

// sub_822DA8B0 -- copy a 0x88-byte state record field by field, r4 -> r3.
// 284 B, 5 callers.
//
// STRUCTURAL. 35 assignments covering 35 distinct offsets, so one match pins
// the whole layout and, more usefully, the TYPE of every field: the load
// instruction names it.
//
//      ld/std     0x00        one 8-byte member
//      lfs/stfs   a float
//      lwz/stw    a 4-byte integer or pointer
//      lbz/stb    a byte
//
// Read straight off the listing, in emitted order:
//
//   00  ld/std          38  lbz     54  lfs     70  lwz
//   08  lfs             39  lbz     58  lfs     74  lwz
//   0C  lfs             3A  lbz     5C  lfs     78  lwz
//   10  lfs             3B  lbz     60  lfs     7C  lwz
//   14  lwz             3C  lfs     64  lwz     80  lwz
//   18  lwz             40  lfs     68  lwz     84  lwz
//   1C..30 six lfs      44  lfs     6C  lwz
//                       48  lfs
//                       4C  lfs
//                       50  lwz
//
// **0x34 IS SKIPPED.** Every other 4-byte slot from 0x00 to 0x84 is copied
// and that one is not, with the copy resuming at 0x38 with a byte. There is
// no alignment reason for a hole there -- 0x38 is a `u8` and needs none --
// so the field exists and the copy deliberately leaves it alone. That is the
// single strongest piece of evidence that this is a hand-written copy and
// not a compiler-generated one: an implicit copy assignment has no way to
// skip a member.
//
// It is also why the 8-byte move at 0x00 is declared as one 8-byte member
// rather than two `u32`s that MSVC merged. If MSVC were merging adjacent
// same-type copies here it would have merged 0x38..0x3B into a single word
// move, and it did not; four separate `lbz`/`stb` pairs say no merging is
// happening, so the `ld`/`std` is a member that really is 64 bits wide.
//
// SIZE IS NOT ASSERTED. The last copied field is at 0x84, and the 8-byte
// member at 0x00 forces 8-byte alignment, which makes 0x88 the smallest
// possible sizeof -- but "smallest possible" is not "measured", and nothing
// here rules out further members past 0x84.

struct StateRec
{
    /* 0x00 */ u64 v00;
    /* 0x08 */ f32 f08;
    /* 0x0C */ f32 f0C;
    /* 0x10 */ f32 f10;
    /* 0x14 */ u32 i14;
    /* 0x18 */ u32 i18;
    /* 0x1C */ f32 f1C;
    /* 0x20 */ f32 f20;
    /* 0x24 */ f32 f24;
    /* 0x28 */ f32 f28;
    /* 0x2C */ f32 f2C;
    /* 0x30 */ f32 f30;
    /* 0x34 */ u32 i34;          /* present, and NOT copied */
    /* 0x38 */ u8  b38;
    /* 0x39 */ u8  b39;
    /* 0x3A */ u8  b3A;
    /* 0x3B */ u8  b3B;
    /* 0x3C */ f32 f3C;
    /* 0x40 */ f32 f40;
    /* 0x44 */ f32 f44;
    /* 0x48 */ f32 f48;
    /* 0x4C */ f32 f4C;
    /* 0x50 */ u32 i50;
    /* 0x54 */ f32 f54;
    /* 0x58 */ f32 f58;
    /* 0x5C */ f32 f5C;
    /* 0x60 */ f32 f60;
    /* 0x64 */ u32 i64;
    /* 0x68 */ u32 i68;
    /* 0x6C */ u32 i6C;
    /* 0x70 */ u32 i70;
    /* 0x74 */ u32 i74;
    /* 0x78 */ u32 i78;
    /* 0x7C */ u32 i7C;
    /* 0x80 */ u32 i80;
    /* 0x84 */ u32 i84;
};
ASSERT_OFFSET(StateRec, v00, 0x00);
ASSERT_OFFSET(StateRec, f08, 0x08);
ASSERT_OFFSET(StateRec, f10, 0x10);
ASSERT_OFFSET(StateRec, i14, 0x14);
ASSERT_OFFSET(StateRec, i18, 0x18);
ASSERT_OFFSET(StateRec, f1C, 0x1C);
ASSERT_OFFSET(StateRec, f30, 0x30);
ASSERT_OFFSET(StateRec, i34, 0x34);
ASSERT_OFFSET(StateRec, b38, 0x38);
ASSERT_OFFSET(StateRec, b3B, 0x3B);
ASSERT_OFFSET(StateRec, f3C, 0x3C);
ASSERT_OFFSET(StateRec, i50, 0x50);
ASSERT_OFFSET(StateRec, f54, 0x54);
ASSERT_OFFSET(StateRec, f60, 0x60);
ASSERT_OFFSET(StateRec, i64, 0x64);
ASSERT_OFFSET(StateRec, i84, 0x84);

void CopyState(StateRec* d, const StateRec* s)
{
    d->v00 = s->v00;
    d->f08 = s->f08;
    d->f0C = s->f0C;
    d->f10 = s->f10;
    d->i14 = s->i14;
    d->i18 = s->i18;
    d->f1C = s->f1C;
    d->f20 = s->f20;
    d->f24 = s->f24;
    d->f28 = s->f28;
    d->f2C = s->f2C;
    d->f30 = s->f30;
    d->b38 = s->b38;
    d->b39 = s->b39;
    d->b3A = s->b3A;
    d->b3B = s->b3B;
    d->f3C = s->f3C;
    d->f40 = s->f40;
    d->f44 = s->f44;
    d->f48 = s->f48;
    d->f4C = s->f4C;
    d->i50 = s->i50;
    d->f54 = s->f54;
    d->f58 = s->f58;
    d->f5C = s->f5C;
    d->f60 = s->f60;
    d->i64 = s->i64;
    d->i68 = s->i68;
    d->i6C = s->i6C;
    d->i70 = s->i70;
    d->i74 = s->i74;
    d->i78 = s->i78;
    d->i7C = s->i7C;
    d->i80 = s->i80;
    d->i84 = s->i84;
}
