// sub_82606EC8 -- 16-byte-aligned bump allocation out of a double-buffered
// arena. When the current buffer cannot satisfy the request the arena flips
// to the other buffer, zeroing a registered list of back-pointers first.
// 160 B, 68 callers.  sub_82606FD8 is the SAME 40 instructions on a second
// arena (82A352AC / 82A352C4 instead of 82A35288 / 82A352A0), 24 callers,
// and matches the same way -- see src/h_arena_twin.cpp.
//
// MATCHED: 35 of 35 non-relocated words, 160 B, at /O2.
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
// Four things had to be right, and each was measured against alternatives:
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
// 4. THE CAPACITY CHECK IS A POINTER DIFFERENCE, NOT AN INTEGER ONE. This is
//    the whole of what was missing, and it is a new lever -- see below.
//
// ---------------------------------------------------------------------
// THE LEVER: A FOLDED-AWAY READ STILL SETS OPERAND ORDER.
//
// For a long time this function sat at 33 of 35, the two wrong words being
//
//      82606F40  want  add r3,r7,r9      got  add r3,r9,r7
//      82606F58  want  add r3,r10,r9     got  add r3,r9,r10
//
// -- same registers, same values, operands transposed. MSVC's rule
// (MATCHED.md, measured on 24 probe functions) is that for `a + b` the
// operand whose SOURCE READ COMES LATER goes in rA. With the guard written
// `g_arena.size - g_arena.cursor < need`, the cursor is read at the top and
// the base only in the tail, so the base is the later read and lands in rA.
// The target has the cursor in rA, so the BASE must be read FIRST.
//
// The recorded conclusion was that this is unreachable, because every way of
// reading the base before the guard keeps a value live across the branch:
// MSVC then hoists the load, merges the two duplicated tails, and the body
// drops from 40 words to 36 or fewer. Six such shapes were measured and all
// six do exactly that (144-152 B, 1 to 4 words right). That measurement is
// correct. The conclusion drawn from it was not.
//
// What it missed is that the base read does not have to SURVIVE. Writing the
// capacity check as a difference of two POINTERS built from the same base
//
//      char* end = g_arena.base + g_arena.size;
//      char* cur = g_arena.base + g_arena.cursor;
//      if (end - cur < need)
//
// reads the base twice, ahead of the cursor, and then folds both reads away
// completely: `(base + size) - (base + cursor)` is `size - cursor`, and the
// emitted guard is still the same four instructions (`lwz`/`lwz`/`subf`/
// `cmpw`) with no load of the base anywhere before the branch. Nothing is
// live across the branch, so the tails stay duplicated and the body stays at
// 40 words -- but the front end has already recorded the base's read as
// earlier than the cursor's, and both adds come out with the cursor in rA.
// 35 of 35, 160 B.
//
// So: **an operand's read position is set before the expression that reads
// it is folded away.** A read that contributes nothing to the emitted code
// still moves an `add`. That is worth trying wherever the read-order rule
// says a value must be read early and reading it early costs code, because
// it is the one way to have both.
//
// Six spellings of the same idea all reach 35 of 35, which is what makes it
// a lever rather than a coincidence: the expression inline, fully
// parenthesised, the two pointers in named locals, the base hoisted into a
// local first (`char* b = g_arena.base; b + size - (b + cursor)`), the
// subtraction spelled `base + size - base - cursor`, and the round trip
// `size - (base + cursor - base)`. The swap written the other way round
// (`other` read first) also matches, so that half carries no information.
//
// Two nearby spellings do NOT match, and they bound it:
//  * an inlined `static char* At(int off) { return g_arena.base + off; }`
//    used as `At(size) - At(cursor)` is 33 of 35 -- the inliner normalises
//    the read positions away, which is the same result MATCHED.md records
//    for `lwzx` operand order and inlining.
//  * writing the guard with `need` on the left (`need > end - cur`) is 33 of
//    35: that moves `need`'s read ahead of the base's and puts the base back
//    in rA.
//
// ---------------------------------------------------------------------
// Ruled out earlier, and still worth not re-trying (all byte-identical to
// the 33-of-35 baseline, all measured):
//
//  * the NAMED CONST VIEW that cracked sub_82667EE0, in five placements: a
//    `const Arena*` driving the size check, the whole body, only the tail's
//    base read, only the tail's cursor read, only the swap's base read. A
//    const view of a GLOBAL is the same `lis`/`addi` address expression, so
//    there is nothing for value numbering to tell apart. Confirmed again
//    this session with three more forms of the same idea -- `Arena* a =
//    &g_arena;`, `Arena& a = g_arena;`, a file-static `Arena* const`, a
//    `const s32*` view of the struct used for the guard and for the tail
//    separately, and a `union { Arena a; s32 w[6]; }` view. All 33 of 35.
//  * the tail addition written `cursor + base`; `&g_arena.base[cursor]`;
//    `&g_arena.cursor[g_arena.base]`; base and other declared as u32 so the
//    addition is a pure integer add, written both ways; the cursor advance
//    written `need + cursor`; the swap stores in the other order.
//  * naming the tail's base and cursor in locals, in either declaration
//    order -- 168 B, 20 of 35. The locals cost two words. Same for a masked
//    index (`base[cursor & 0x3FFFFFFF]`, 168 B) and for writing the advance
//    before the pointer (168 B): each forces the cursor into its own
//    register across the branch.
//  * reading the base early into anything that stays live: 144-152 B, 1 to 4
//    of 32.
//  * flagsweep: 72 combinations, two outcomes only -- 33/40 at 160 B for 44
//    of them, 18/40 at 148 B for the 28 that include /Os.

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

// A KNOWN NAME COLLISION, recorded rather than papered over. Putting this
// function in the manifest made build.py's link check report
//
//     WOULD NOT LINK: 1 symbol(s) resolve to more than one address.
//       ?g_arena@@3UArena@@A
//           829A195C  referenced from 822DF658
//           82A35288  referenced from 82606ED0, 82606FA4
//
// Every byte still verifies -- each relocation site is solved from the image
// independently, so a splice cannot see this -- but one name cannot have two
// addresses and a real link would refuse it.
//
// It is NOT this file's to fix, and that the check fires without these rows
// is MEASURED, not assumed: a build with this file renamed to `g_arena1`
// reported the same collision with only 82606FA4 -- y1_arena_flip.cpp's
// site -- on the 82A35288 side.
// `src/m63_sum_below_limit.cpp` uses the name `g_arena` for a
// DIFFERENT object at 829A195C, while `src/y1_arena_flip.cpp` (82606F68,
// the reference at 82606FA4) uses it for THIS one at 82A35288, and both were
// already in the tree. Renaming here to `g_arena1` was tried and is worse:
// it silences nothing -- the m63/y1 pair still collides -- and it gives one
// object at 82A35288 two different names across two files, which build.py
// does NOT report for data and which would therefore be a model error that
// hides. So this file agrees with the other file that names the same arena,
// and the remaining collision stays visible and attributable to the two
// files that actually disagree.
extern Arena     g_arena;
extern FixupList g_fixups;

void* ArenaAlloc(int n)
{
    int need = (n + 15) & ~15;

    char* end = g_arena.base + g_arena.size;
    char* cur = g_arena.base + g_arena.cursor;

    if (end - cur < need)
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
