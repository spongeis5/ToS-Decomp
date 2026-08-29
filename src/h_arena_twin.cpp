#include "types.h"

// sub_82606FD8 -- the SECOND arena. Byte-for-byte the same 40 instructions as
// sub_82606EC8 (src/f_arena_alloc.cpp), naming 82A352AC / 82A352C4 where that
// one names 82A35288 / 82A352A0. 160 B, 24 callers.
//
// NOT MATCHED, and it is the same stall, at the same two words:
//
//      82607050  want  add r3,r7,r9      got  add r3,r9,r7
//      82607068  want  add r3,r10,r9     got  add r3,r9,r10
//
//      40 word(s) compared: 33 identical, 2 differ, 5 differ in a relocated
//      word (expected)
//
// Same registers, same values, operands transposed: the target puts the
// CURSOR in rA and every source shape tried puts the BASE there. Read
// f_arena_alloc.cpp for the full account -- the eleven shapes tried there,
// the 72-combination flag sweep, and why any source that reads the base
// before the branch collapses the two tails into one and drops the body from
// 40 words to 37.
//
// NINE MORE SHAPES were tried on this twin, all 33 of 35 with exactly those
// two words transposed and nothing else moved:
//
//   * base and cursor named in locals, base first, then `b + c`
//   * `char* p = g.base; p += g.cursor;`  (compound assignment)
//   * `g.cursor + g.base`  (integer-first pointer arithmetic)
//   * one level of inlined helper, `Take(&g_arena, need)`
//   * TWO levels of inlined helper -- `Take` calling `At(a)` -- which is the
//     shape that cracked sub_82164040 and sub_82703E28 in the same session
//   * base/other held as u32 rather than char*, with the sum written both
//     ways round, so the add is an integer add whose operand order
//     MATCHED.md records as readable off the source
//   * the fast path written as an early return with its own tail
//     (that one is worse: 38 words, the guard inverts)
//
// The integer-field pair is the informative one. `a + b` on two ints is the
// case where operand order IS supposed to be readable, and BOTH orders --
// `g.cursor + g.base` and `g.base + g.cursor` -- compile to the same
// transposed `add`. So the choice here is not being made from the source
// expression at all.
//
// What the session did establish, from sub_826C0F50, is that the choice moves
// with the DECLARATION ORDER OF THE LOCALS rather than the order of the reads
// in the expression: `NthNode* q = n; s32 r = index - total;` and the same
// two declarations swapped give `add r3,r11,r10` and `add r3,r10,r11` from a
// character-for-character identical return statement. There are no locals to
// reorder here -- both operands are global fields whose CSE representatives
// are fixed by the guard -- which is consistent with this being unreachable
// from the source and is the reason to stop.
//
// So: same class as MATCHED.md's "What still resists". One operand-selection
// decision, reachable from neither source order nor flags.

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

    if (g_arena2.size - g_arena2.cursor < need)
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
