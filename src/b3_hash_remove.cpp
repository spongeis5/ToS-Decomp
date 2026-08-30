#include "types.h"

// sub_826377B0 -- remove one slot from an open-addressed table and close the
// gap by shifting later entries back over it. 296 B, 6 callers.
//
// NEAR MISS: 71 of 74 words (was 69), exact size (296 B), every instruction
// and every register correct. The three that differ are ALL the rA/rB operand
// order of an indexed address, and the address computed is identical either
// way:
//
//   82637824  want lwzx r6,r8,r9    got lwzx r6,r9,r8   for-loop peel test
//   82637840  want lwzx r7,r8,r9    got lwzx r7,r9,r8   the hash's key read
//   82637894  want add  r7,r8,r9    got add  r7,r9,r8   &slots[i] for .val
//
// All three want the BASE (r8, the reloaded `slots`) in rA; every spelling
// tried puts the INDEX there.
//
// ---------------------------------------------------------------------
// WHAT MOVED, AND WHY: the AND-MASK lever, applied to ONE site.
//
// This file previously recorded five wrong words and the conclusion that
// none of them was reachable, because retail's `lwzx` operand order is not
// uniform within the function. The first half of that is measured and still
// true. The conclusion was wrong: two of the five come out with a
// one-character change.
//
//      slots[hole & 0x1FFFFFFFu].key = -1;      // the FIRST store, before
//                                               // the cluster walk
//
// fixes BOTH 8263788C (`stwx r7,r11,r8`) and 82637898 (`add r11,r11,r8`) --
// the two `slots[hole]` accesses INSIDE the loop, which this statement does
// not contain. Size is unchanged at 296 B: with 8-byte elements the scaling
// is `rlwinm rX,rY,3,0,28`, which already discards everything above bit 28,
// so a 29-bit mask is absorbed and no instruction appears for it. (0x3FFFFFFF
// is byte-identical for the same reason -- the extra bit is shifted out.)
//
// The mechanism, and it is the MATCHED.md lever read one level up: MSVC
// matches `base + (index << scale)` as an addressing mode and puts the base
// in rA; a masked index misses the pattern, falls back to a generic add, and
// puts the index in rA. What is new here is that the choice is made ONCE per
// index EXPRESSION and then applies at every use of it. `hole << 3` is one
// value number shared by this store and the two in-body accesses, so masking
// the first occurrence re-classifies all three. Masking the in-body
// occurrences INSTEAD (or as well) puts it back to 69 of 74 and costs 4
// bytes -- so it is specifically the first, and only the first.
//
// So: **when an indexed access has the wrong operand order, mask the FIRST
// occurrence of that index expression, not the one that is wrong.**
//
// ---------------------------------------------------------------------
// WHAT IS LEFT, stated as measured rather than as a verdict.
//
// The three remaining words are the reads at index `i`. `i` is defined by
// `(x) & mask` -- it is already a masked value -- so every access through it
// takes the generic-add form, and the mask lever cannot be un-applied. In
// the retail function the SAME source expression comes out both ways:
// `slots[i].key != -1` is base-first at the loop peel (82637824) and
// index-first at the loop bottom (826378C0), which is a single expression
// that MSVC's loop rotation duplicated. No spelling of that one expression
// can differ between its two copies.
//
// Splitting it into two source expressions was the obvious answer and it
// does not work: writing the peel as its own `s32 k = slots[i].key;` with
// the loop rewritten as `while (k != -1) { ... k = slots[i].key; }` and the
// three guards as `goto next` reorganises the whole body -- 288 B and 17 of
// 72, and the eleven masked/byte-arithmetic variants of it are all between
// 10 and 50 of 74. The `for` with `continue` is the shape that produces the
// target's branch structure and nothing else comes close.
//
// Measured this session, all 296 B and all 71 of 74 with exactly those three
// words -- the residue is remarkably insensitive to spelling:
//
//   * `i[slots].key`, the REVERSED SUBSCRIPT, at each of the nine subscript
//     sites and in seven combinations. Byte-identical to `slots[i].key`
//     everywhere: MSVC canonicalises it in the front end, so it cannot be
//     used to move a source read position the way `b + c` versus `c + b`
//     sometimes can. That is a clean negative and worth not re-trying.
//   * inlined accessors at the three sites: a `Slot8&` reference accessor,
//     a `Slot8*` pointer accessor, an `s32` key accessor, `operator[]`,
//     `__forceinline`, a `const` accessor, and a FREE `KeyOf(slots, i)`
//     helper -- 13 variants, every one byte-identical. (MATCHED.md's
//     sub_8215ED28 note says inlining normalises an indexed address to
//     base-first; here it normalises to index-first instead, so the
//     direction of that normalisation is a property of the function, not of
//     the inliner.)
//   * `(slots + i)->key`, `(&slots[0])[i].key`, `this->slots[i].key`,
//     `SlotBase::slots[i]` through a base class, `*(s32*)((char*)slots +
//     i * 8)`, `*(s32*)((char*)slots + (i << 3))`, `slots[(s32)i]`, and a
//     `u32 x = i;` copy used at the three sites.
//   * the .val copy through a temporary (`s32 v = slots[i].val;`) and
//     through named `src`/`dst` element pointers.
//   * STATEMENT ORDER in the body: the destination index named in a local,
//     the source index named in a local, and `hole = i;` moved ahead of the
//     final clear (`slots[hole].key = -1` after the assignment). All three
//     are 296 B and 71 of 74. Swapping the .key and .val copies is worse
//     (300 B, 34 of 74 -- the key store no longer reuses the hash's loaded
//     value), and so is saving the key and clearing the slot first (292 B,
//     4 of 73) or naming the old hole in `u32 old = hole;` (296 B, 53).
//
// Worse, and why: a whole-struct `slots[hole] = slots[i];` (288 B, 29 of 70
// -- it emits its own pair of loads instead of reusing the hash's), and
// `Slot8* e = &slots[i];` hoisted for the body (288 B, 36 of 70).
//
// Earlier measurements, still valid:
//  * FREE function -- 67 of 74. It flips exactly 826378A8 and 826378C0 to
//    base-first, which are the two the member form gets right. Free plus the
//    S1 mask is 69.
//  * pointer arithmetic instead of the subscript -- identical to plain.
//  * declaring `i`/`end` ahead of `j` -- no change.
//  * the three guards folded into one positive `if` with `&&` of negations,
//    and as a `while` with an explicit `goto next` step -- no change.
//  * tools/flagsweep.py, 72 combinations including /Ou prescheduling: best
//    is plain /O2 /Gy /GS- /fp:fast. /Os is 13 of 74 at 284 B.
//
// Everything else reads straight off the listing. Slot layout is forced by
// `rlwinm rX,rY,3,0,28` on every index and a `4(rX)` displacement on the
// second word: 8 bytes, key at +0, payload at +4. `cmpwi cr6,rX,-1` is
// SIGNED and is the only encodable form for -1, so the empty marker is s32.
//
//   lwz r11,4(r3) ; addi r8,r11,-1 ; stw r8,4(r3)     count--
//   stwx r31,r10,r9                                   slots[hole].key = -1
//   lwz r8,0(r3)                                      RELOAD: the store may
//                                                     alias this->slots
//   add r7,r11,r4 ; and r10,r7,r11                    j = (hole + mask) & mask
//   ... walk BACK to the empty slot that ends the cluster ...
//   addi r9,r4,1 ; addi r7,r10,1                      i = hole+1, end = j+1
//   lis/ori 9E3779B1                                  hoisted, loop-invariant
//
// `(x + mask) & mask` is `x - 1` modulo the power-of-two size, and it is
// written that way in the source: the mask arrives in a REGISTER and is
// added (`add r7,r11,r4`), where `(x - 1) & mask` would be `addi rX,r4,-1`.
//
// The three guards each branch FORWARD to the loop's own increment, which is
// what `continue` inside a `for` produces -- the increment is in the third
// clause, so a `while` with the step at the bottom would not reach it. Read
// straight off the branch targets:
//
//   blt- A          -> if (i >= end && h > hole) continue;
//   bgt- CONT
//   A: bge- B       -> if (i < hole && (h > hole || h <= i)) continue;
//      bgt- CONT
//      ble- CONT
//   B: ble- C       -> if (h > hole && h < end) continue;
//      blt- CONT
//   C: move
//
// Every comparison in the loop is `cmplw` (unsigned) while the empty test is
// `cmpwi` (signed), so the indices are u32 and the key is s32; the hash shift
// is `rlwinm ...,28,4,31`, a LOGICAL right shift, hence the cast.
//
// -1 lives in r31 across the whole body because it is stored twice, once
// before the loop and once inside it; r30 holds the hash product. Those two
// callee-saved registers are the whole prologue.
//
// WHAT THE MASK DOES AND DOES NOT ASSERT: `hole & 0x1FFFFFFF` names the same
// slot as `hole` for every index whose byte offset fits in 32 bits, which is
// every index this table can hold, so it changes nothing the function
// computes. It is not evidence that the original source contained a mask --
// the bits it keeps are exactly the bits the `* 8` scaling keeps, so the
// instruction is the same either way. It is a way of reaching the operand
// order, in the same sense as sub_8215ED28 and sub_826DD4A0 in MATCHED.md.

struct Slot8
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 val;
};
ASSERT_SIZE(Slot8, 8);

struct HashTable8
{
    /* 0x00 */ Slot8* slots;
    /* 0x04 */ s32    count;
    /* 0x08 */ u32    mask;

    void RemoveAt(u32 hole);
};
ASSERT_OFFSET(HashTable8, count, 0x04);
ASSERT_OFFSET(HashTable8, mask,  0x08);

void HashTable8::RemoveAt(u32 hole)
{
    count = count - 1;
    slots[hole & 0x1FFFFFFFu].key = -1;

    u32 j = (hole + mask) & mask;
    while (slots[j].key != -1)
        j = (j + mask) & mask;

    u32 i = (hole + 1) & mask;
    u32 end = (j + 1) & mask;

    for (; slots[i].key != -1; i = (i + 1) & mask)
    {
        u32 h = (((u32)slots[i].key >> 4) * 0x9E3779B1u) & mask;

        if (i >= end && h > hole)
            continue;
        if (i < hole && (h > hole || h <= i))
            continue;
        if (h > hole && h < end)
            continue;

        slots[hole].key = slots[i].key;
        slots[hole].val = slots[i].val;
        slots[i].key = -1;
        hole = i;
    }
}
