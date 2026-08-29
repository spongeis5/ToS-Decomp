// sub_8219EB58 -- open-addressed table lookup, capacity a power of two,
// linear probing all the way round to the starting slot. 96 B, 84 callers.
//
//   lwz    r8,0(r3)           n     = t->capacity
//   mr     r11,r3             t moves out of r3 ...
//   li     r3,0               ... because r3 holds the result from the start
//   addi   r10,r8,-1          n - 1
//   and    r7,r10,r4          start = (n - 1) & key
//   lwz    r9,4(r11)          t->slots, hoisted out of the loop
//   mr     r11,r7             i = start
// L:rlwinm r10,r11,2,0,29     i * 4
//   lwzx   r6,r10,r9          t->slots[i]
//   cmplwi cr6,r6,0
//   beq-   cr6,next
//   rotlwi r10,r6,0
//   lwz    r6,4(r10)
//   cmplw  cr6,r6,r4
//   beq-   cr6,found
// next:
//   addi   r11,r11,1
//   subfc  r10,r8,r11         CA = (i >= n) unsigned
//   subfe  r5,r6,r6           0 when CA, else -1
//   and    r11,r5,r11         i = (i >= n) ? 0 : i
//   cmplw  cr6,r11,r7
//   bne+   cr6,L              while (i != start)
//   blr                       falls out with r3 still 0
// found:
//   mr     r3,r10
//   blr
//
// The wrap is the subfc/subfe borrow trick, not arithmetic the source spells
// out: `subfc r10,r8,r11` sets carry when i >= n, and `subfe r5,r6,r6` with
// one register used three times turns that carry into 0 or -1 with no branch.
// ANDing i with it snaps i to zero exactly when it reached the capacity. The
// register it reuses (r6) is dead on both edges into `next` -- the loaded key
// on one, zero on the other -- which is why the same register can serve.
//
// Three things had to be right together, and each is visible in the listing:
//
// 1. `t->slots[i]` is WRITTEN OUT each time rather than held in a local. The
//    `rotlwi r10,r6,0` -- a register move spelled as a rotate -- is the
//    common subexpression being copied. With `BucketEntry* e = t->slots[i];`
//    the compiler loads straight into the return register and the move
//    disappears, along with 4 words.
//
// 2. ONE result variable and a `break`, not two `return`s. Two returns let
//    the compiler put the entry in r3 and leave with `beqlr`; the target
//    instead sets r3 = 0 in the third instruction, keeps it live across the
//    whole loop, and pays for an out-of-line `mr r3,r10 ; blr`. That is what
//    a single-exit function looks like after the exit block is duplicated.
//    It also forces `t` out of r3 into r11, which is where `mr r11,r3` and
//    `lwz r9,4(r11)` come from.
//
// 3. `start` is computed FIRST and `i` initialised from it. The target reads
//    `and r7,r10,r4` then `mr r11,r7` -- start into r7, copied into i. Written
//    as `i = ...; start = i;` the copy goes the other way and the registers
//    swap.

#include "types.h"

struct BucketEntry
{
    u32 unk0000;
    u32 key;
};
ASSERT_OFFSET(BucketEntry, key, 0x04);

struct BucketTable
{
    u32           capacity;
    BucketEntry** slots;
};
ASSERT_OFFSET(BucketTable, slots, 0x04);

BucketEntry* BucketFind(BucketTable* t, u32 key)
{
    u32          n = t->capacity;
    u32          start = (n - 1) & key;
    u32          i = start;
    BucketEntry* found = 0;

    do
    {
        if (t->slots[i] != 0 && t->slots[i]->key == key)
        {
            found = t->slots[i];
            break;
        }
        ++i;
        if (i >= n)
            i = 0;
    } while (i != start);

    return found;
}
