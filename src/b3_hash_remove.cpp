#include "types.h"

// sub_826377B0 -- remove one slot from an open-addressed table and close the
// gap by shifting later entries back over it. 296 B, 6 callers.
//
// NEAR MISS: 69 of 74 words, exact size (296 B), every instruction and every
// register correct. The five that differ are ALL the rA/rB operand order of
// an indexed address, and the address computed is identical either way:
//
//   82637824  want lwzx r6,r8,r9    got lwzx r6,r9,r8     peeled loop test
//   82637840  want lwzx r7,r8,r9    got lwzx r7,r9,r8     the hash's key read
//   8263788C  want stwx r7,r11,r8   got stwx r7,r8,r11    slots[hole].key = k
//   82637894  want add  r7,r8,r9    got add  r7,r9,r8     &slots[i]
//   82637898  want add  r11,r11,r8  got add  r11,r8,r11   &slots[hole]
//
// This is the sub_8215ED28 address-selection stall, and it is not uniform in
// the TARGET either: the same expression `slots[i].key` is base-first at the
// peel (82637824) and index-first at the loop bottom (826378C0), so no single
// convention in the source can produce both.
//
// What was measured, not guessed:
//  * FREE function -- 67 of 74. All SEVEN indexed operand pairs inverted.
//  * MEMBER function -- 69 of 74. Flips exactly two of them (826378A8 and
//    826378C0) into agreement and nothing else. Kept.
//  * pointer arithmetic `(t->slots + i)->key` instead of the subscript -- 67,
//    identical to the free form.
//  * declaring `i`/`end` ahead of `j` (the declaration-order lever) -- 69,
//    same five words.
//  * the three guards folded into one positive `if` with `&&` of negations,
//    and again as a `while` with an explicit `goto next` step -- 69 both,
//    same five words.
//  * an inlined member accessor `Slot8* At(u32 x) { return &slots[x]; }`,
//    used for the reads at `i` only and then for every access -- 69 both,
//    the SAME five words, which is the MATCHED.md result that inlining
//    normalises the operand order rather than carrying it.
//  * tools/flagsweep.py, 72 combinations including /Ou prescheduling: best is
//    69 of 74 at plain /O2 /Gy /GS- /fp:fast. /Os is 13 of 74 at 284 B.
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
    slots[hole].key = -1;

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
