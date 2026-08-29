// sub_82637688 -- Knuth-multiplicative hash, linear probe, returns the SLOT
// INDEX of a key or capacity (mask + 1) when the probe hits an empty slot.
// 84 B, 20 callers.
//
//      lis     r8,-25033
//      lwz     r10,8(r3)               t->mask
//      rlwinm  r11,r4,28,4,31          key >> 4
//      lwz     r9,0(r3)                t->entries
//      ori     r7,r8,31153             0x9E3779B1
//      mullw   r6,r11,r7
//      and     r3,r6,r10               i = hash & mask     (result register)
//      rlwinm  r5,r3,3,0,28            i * 8
//      lwzx    r11,r5,r9
//      cmpwi   cr6,r11,-1
//      beq-    cr6,empty
//  L:  cmplw   cr6,r11,r4
//      beqlr   cr6                     found: i is already in r3
//      addi    r11,r3,1
//      and     r3,r11,r10              i = (i + 1) & mask
//      rlwinm  r8,r3,3,0,28
//      lwzx    r11,r8,r9
//      cmpwi   cr6,r11,-1
//      bne+    cr6,L
// empty:
//      addi    r3,r10,1
//      blr
//
// The empty test is peeled ahead of the loop and repeated on the back edge,
// which is a rotated `while`, not a do/while: the do/while rule says the
// loop top would otherwise be reached by fall-through with no copy in front.
//
// The index lives in r3 for the whole loop -- it is the return value and the
// probe cursor at once, which is why `and` writes r3 both times.
//
// mask and entries are loaded once and never reloaded: nothing in the body
// stores, so this reads as a const query on the table.
//
// Stride 8 (`rlwinm ...,3,0,28`) with the compared word at offset 0 makes an
// entry a key/value pair; 0xFFFFFFFF is the empty marker, and it is compared
// with `cmpwi ...,-1` because the constant will not fit a cmplwi immediate.

#include "types.h"

struct HashEntry
{
    /* 0x00 */ u32 key;
    /* 0x04 */ u32 value;
};

ASSERT_OFFSET(HashEntry, value, 0x04);
ASSERT_SIZE(HashEntry, 8);

struct HashTable
{
    /* 0x00 */ HashEntry* entries;
    /* 0x04 */ u32        unk0004;
    /* 0x08 */ u32        mask;
};

ASSERT_OFFSET(HashTable, entries, 0x00);
ASSERT_OFFSET(HashTable, mask, 0x08);

u32 HashFindSlot(HashTable* t, u32 key)
{
    u32 i = ((key >> 4) * 0x9E3779B1u) & t->mask;

    while (t->entries[i].key != 0xFFFFFFFFu)
    {
        if (t->entries[i].key == key)
            return i;
        i = (i + 1) & t->mask;
    }

    return t->mask + 1;
}
