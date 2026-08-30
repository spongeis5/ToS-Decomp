#include "types.h"

// sub_82606FD8 -- the SECOND arena. Byte-for-byte the same 40 instructions as
// sub_82606EC8 (src/f_arena_alloc.cpp), naming 82A352AC / 82A352C4 where that
// one names 82A35288 / 82A352A0. 160 B, 24 callers.
//
// MATCHED: 35 of 35 non-relocated words, 160 B, at /O2 -- by the same lever,
// on the same two words that held both twins for so long:
//
//      82607050  want  add r3,r7,r9      got  add r3,r9,r7
//      82607068  want  add r3,r10,r9     got  add r3,r9,r10
//
// Same registers, same values, operands transposed: the target puts the
// CURSOR in rA and every source shape that computes the capacity check as an
// INTEGER difference puts the BASE there, because MSVC gives rA to the
// operand whose source read comes later and `size - cursor` reads the cursor
// at the top while the base is only read in the tail.
//
// The fix, in full in f_arena_alloc.cpp, is that the guard is a POINTER
// difference:
//
//      char* end = g_arena2.base + g_arena2.size;
//      char* cur = g_arena2.base + g_arena2.cursor;
//      if (end - cur < need)
//
// Both base reads fold away -- `(b + size) - (b + cursor)` is `size -
// cursor`, and the emitted guard is unchanged, four instructions with no
// load of the base before the branch -- but the read POSITION survives the
// folding, and that is what the operand-order rule reads. Nothing stays live
// across the branch, so the two duplicated tails survive and the body stays
// at 40 words.
//
// This twin is the control for that claim. It was written down as the same
// stall for the same recorded reason, was never edited toward the answer,
// and the identical one-line change takes it from 33 of 35 to 35 of 35 with
// nothing else touched.
//
// Ruled out here before the lever was found, all 33 of 35 with exactly those
// two words transposed and nothing else moved:
//
//   * base and cursor named in locals, base first, then `b + c`
//   * `char* p = g.base; p += g.cursor;`  (compound assignment)
//   * `g.cursor + g.base`  (integer-first pointer arithmetic)
//   * one level of inlined helper, `Take(&g_arena2, need)`
//   * TWO levels of inlined helper -- `Take` calling `At(a)` -- the shape
//     that cracked sub_82164040 and sub_82703E28
//   * base/other held as u32 rather than char*, with the sum written both
//     ways round, so the add is a pure integer add
//   * the fast path written as an early return with its own tail
//     (worse: 38 words, the guard inverts)
//
// The integer-field pair is why the earlier conclusion looked so solid.
// `a + b` on two ints is the case where operand order IS supposed to be
// readable off the source, and BOTH orders compiled to the same transposed
// `add` -- so the choice was demonstrably not being made from the source
// expression. That measurement was right. What it did not show, and what was
// wrongly inferred from it, is that the choice was unreachable: it is made
// from the read POSITIONS, and a read that the optimiser deletes still has
// one.

struct Arena2
{
    /* 0x00 */ char* base;
    /* 0x04 */ char* other;
    /* 0x08 */ s32   size;
    /* 0x0C */ s32   cursor;
    /* 0x10 */ s32   unk0010;
    /* 0x14 */ s32   unk0014;
};

ASSERT_OFFSET(Arena2, base,    0x00);
ASSERT_OFFSET(Arena2, other,   0x04);
ASSERT_OFFSET(Arena2, size,    0x08);
ASSERT_OFFSET(Arena2, cursor,  0x0C);
ASSERT_OFFSET(Arena2, unk0010, 0x10);
ASSERT_OFFSET(Arena2, unk0014, 0x14);

struct FixupList2
{
    /* 0x00 */ s32   count;
    /* 0x04 */ s32** slots;
};

ASSERT_OFFSET(FixupList2, count, 0x00);
ASSERT_OFFSET(FixupList2, slots, 0x04);

extern Arena2     g_arena2;
extern FixupList2 g_fixups2;

void* Arena2Alloc(int n)
{
    int need = (n + 15) & ~15;

    char* end = g_arena2.base + g_arena2.size;
    char* cur = g_arena2.base + g_arena2.cursor;

    if (end - cur < need)
    {
        int   c = g_fixups2.count;
        s32** q = g_fixups2.slots;
        s32** e = q + c;

        while (q != e)
        {
            **q = 0;
            ++q;
        }

        g_arena2.unk0014 = g_arena2.cursor;
        g_arena2.unk0010 = 0;
        g_arena2.cursor  = 0;
        char* t = g_arena2.base;
        g_arena2.base  = g_arena2.other;
        g_arena2.other = t;
    }

    char* p = g_arena2.base + g_arena2.cursor;
    g_arena2.cursor = g_arena2.cursor + need;
    return p;
}
