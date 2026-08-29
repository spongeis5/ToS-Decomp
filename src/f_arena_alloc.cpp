// sub_82606EC8 -- 16-byte-aligned bump allocation out of a double-buffered
// arena. When the current buffer cannot satisfy the request the arena flips
// to the other buffer, zeroing a registered list of back-pointers first.
// 160 B, 68 callers.  sub_82606FD8 is the SAME 40 instructions on a second
// arena (82A352AC / 82A352C4 instead of 82A35288 / 82A352A0), 24 callers,
// and scores identically against this source.
//
// NOT MATCHED: 33 of 35 non-relocated words, correct size, at /O2. The two
// words that differ are described at the bottom -- they are the operand
// order of two `add` instructions and nothing else.
//
//      lis     r11,-32093
//      addi    r10,r3,15
//      addi    r11,r11,21128           &g_arena
//      rlwinm  r6,r10,0,0,27           need = (n + 15) & ~15
//      lwz     r10,12(r11)             g_arena.cursor
//      lwz     r9,8(r11)               g_arena.size
//      subf    r9,r10,r9               size - cursor
//      cmpw    cr6,r9,r6
//      bge-    cr6,fast
//      lis     r9,-32093
//      li      r7,0
//      addi    r5,r9,21152             &g_fixups
//      lwz     r8,21152(r9)            g_fixups.count
//      lwz     r9,4(r5)                g_fixups.slots
//      rlwinm  r8,r8,2,0,29
//      add     r8,r8,r9                end = slots + count
//      cmplw   cr6,r9,r8
//      beq-    cr6,done
//  L:  lwz     r10,0(r9)
//      addi    r9,r9,4
//      cmplw   cr6,r9,r8
//      stw     r7,0(r10)               **q = 0
//      bne+    cr6,L
//      lwz     r10,12(r11)
// done:stw     r10,20(r11)             unk0014 = cursor
//      mr      r10,r7                  the cursor's new value: 0
//      lwz     r9,4(r11)               g_arena.other
//      lwz     r8,0(r11)               g_arena.base
//      add     r10,r10,r6              0 + need
//      stw     r7,16(r11)              unk0010 = 0
//      add     r3,r7,r9                0 + (the new base)
//      stw     r10,12(r11)             cursor = 0 + need
//      stw     r9,0(r11)               base  = other
//      stw     r8,4(r11)               other = base
//      blr
// fast:lwz     r9,0(r11)
//      add     r3,r10,r9               base + cursor
//      add     r10,r10,r6
//      stw     r10,12(r11)
//      blr
//
// Three things had to be right, and each was measured against alternatives:
//
// 1. The zeroing loop is a POINTER WALK with a precomputed end, not an
//    indexed loop. `for (i = 0; i < g_fixups.count; ++i) *g_fixups.slots[i]
//    = 0;` reloads BOTH fields every iteration -- the store `**q = 0` goes
//    through a pointer that could alias the list -- and comes out as an
//    lwzx loop with the count re-read. Naming count and slots in locals and
//    walking to `end` gives the target's cmplw/bne pair exactly.
//
// 2. `g_arena.cursor = 0;` is written as its own statement and the tail then
//    reads `g_arena.cursor` back. That store is DEAD (the tail overwrites
//    offset 12 with 0 + need) so it is eliminated, but the register that
//    held the zero is forwarded to the reads -- which is where `mr r10,r7`
//    and `add r3,r7,r9` come from. Every spelling that makes the zero a
//    LOCAL instead (`off = 0; ... off + need`) is constant-folded to
//    `stw r6,12(r11)` and loses two instructions. Nine such spellings were
//    tried, including putting `off = 0` on the far side of the loop from its
//    uses; only the memory round-trip survives folding.
//
// 3. The store to offset 12 lands BEFORE the two swap stores even though the
//    swap is written first in the source. MSVC schedules a store across
//    other stores to the same object when the offsets are distinct, so
//    "store order is source order" has this exception on top of the address-
//    computation one already recorded in MATCHED.md.
//
// ---------------------------------------------------------------------
// WHAT IS LEFT: two words, both `add`, both the same choice.
//
//      82606F40  want  add r3,r7,r9      got  add r3,r9,r7
//      82606F58  want  add r3,r10,r9     got  add r3,r9,r10
//
// Same registers, same values, operands transposed. MSVC's rule was measured
// on 24 probe functions: for `a + b` the operand whose SOURCE READ COMES
// LATER goes in rA. Here the cursor is read at the top (the size check) and
// the base only in the tail, so the base is the later read and MSVC puts it
// in rA. The target has the cursor in rA, which means the base was read
// FIRST in the original source.
//
// That is reproducible in isolation -- a probe with `char* b = g.base;`
// ahead of the size check emits `add r3,cursor,base`, and MSVC still SINKS
// the load past the branch, so the emitted schedule is unchanged. It cannot
// be reproduced here: any source that reads the base before the branch has
// to keep that value live across the if-body (the swap consumes it), so
// MSVC hoists the load above the branch instead of sinking it, merges the
// two tails into one, and the body drops from 40 words to 37. Eleven such
// shapes were tried (base local, base+other locals, swap through the local,
// swap re-reading the field, two returns, and an inlined Take(arena, need)
// helper); every one either scores 33/35 with the transposed adds or 37
// words with a merged tail.
//
// flagsweep: 72 combinations, two outcomes only -- 33/40 at 160 B for 44 of
// them, 18/40 at 148 B for the 28 that include /Os. No flag moves it.
//
// So this is the same class as the six entries in MATCHED.md's "What still
// resists": one operand-selection decision, reachable from neither source
// order nor flags.

#include "types.h"

struct Arena
{
    /* 0x00 */ char* base;
    /* 0x04 */ char* other;
    /* 0x08 */ s32   size;
    /* 0x0C */ s32   cursor;
    /* 0x10 */ s32   unk0010;
    /* 0x14 */ s32   unk0014;
};

ASSERT_OFFSET(Arena, base, 0x00);
ASSERT_OFFSET(Arena, other, 0x04);
ASSERT_OFFSET(Arena, size, 0x08);
ASSERT_OFFSET(Arena, cursor, 0x0C);
ASSERT_OFFSET(Arena, unk0010, 0x10);
ASSERT_OFFSET(Arena, unk0014, 0x14);

struct FixupList
{
    /* 0x00 */ s32   count;
    /* 0x04 */ s32** slots;
};

ASSERT_OFFSET(FixupList, count, 0x00);
ASSERT_OFFSET(FixupList, slots, 0x04);

extern Arena     g_arena;
extern FixupList g_fixups;

void* ArenaAlloc(int n)
{
    int need = (n + 15) & ~15;

    if (g_arena.size - g_arena.cursor < need)
    {
        int   c = g_fixups.count;
        s32** q = g_fixups.slots;
        s32** e = q + c;

        while (q != e)
        {
            **q = 0;
            ++q;
        }

        g_arena.unk0014 = g_arena.cursor;
        g_arena.unk0010 = 0;
        g_arena.cursor  = 0;
        char* t = g_arena.base;
        g_arena.base  = g_arena.other;
        g_arena.other = t;
    }

    char* p = g_arena.base + g_arena.cursor;
    g_arena.cursor = g_arena.cursor + need;
    return p;
}
