// sub_82631F78 -- find an interior key in an array of pairs and return a
// field of the paired object. 80 bytes.
//
// *** UNVERIFIED, AND FOR A REASON THAT IS NOT ABOUT THIS SOURCE. ***
// 82631F78 IS NOT IN THE INVENTORY. build/functions_all.txt has one row at
// 82631F30 with size 152, from `discover`, and the next row is 82631FC8 from
// `.pdata`. 82631F30 + 72 = 82631F78, and a second frameless body starts
// there: 72 + 80 = 152 accounts for the row exactly. match.py refuses the
// address outright --
//
//     82631F78 is not a known function start.
//
// -- so this file has NOT been compiled and compared, and it is in
// attempts.txt rather than the manifest. The first body IS matched, 18 of 18,
// as src/n15_slot_clear.cpp.
//
// Regenerating the inventory would settle it (`python tools/inventory.py
// --addrtaken` produces 31,882 rows against the 30,630 in use), but that
// rewrites a file every other running agent reads, and HANDBOOK.md records that
// the two inventories differ in both directions. So the gap is reported
// rather than papered over; the fix belongs to whoever owns the inventory.
//
//      lwz    r9,100(r3)      count, an s32 at +0x64 (lwz, and cmpwi SIGNED)
//      li     r10,0
//      cmpwi  cr6,r9,0
//      ble-   cr6,zero        the peeled `i < count` with i = 0
//      lwz    r7,96(r3)       pairs, +0x60
//      addi   r8,r4,16        the key is the ARGUMENT PLUS 16
//      addi   r11,r7,4        &pairs[0].key -- the SECOND word of the pair
// loop: lwz   r6,0(r11)
//      cmplw  cr6,r6,r8
//      beq-   cr6,hit
//      addi   r10,r10,1
//      addi   r11,r11,8       stride 8, so the element is two words
//      cmpw   cr6,r10,r9
//      blt+   cr6,loop
// zero:li     r3,0
//      blr
// hit: rlwinm r11,r10,3,0,28
//      lwzx   r10,r11,r7      pairs[i].obj -- the FIRST word
//      lwz    r3,8(r10)
//      blr
//
// `addi r8,r4,16` before the loop is the fact worth keeping: what is compared
// is not the argument but a fixed 16 bytes past it, so the table stores an
// INTERIOR pointer to a sub-object of the argument rather than the argument.
//
// The hit block sits after the zero return and is reached by a forward
// `beq-`, so the zero path is the fall-through and the interesting arm is
// out of line -- MATCHED.md's merged-exit reading (sub_8217E808) seen from
// the other side.
//
// The count is compared with `cmpwi`, signed; the key with `cmplw`, unsigned.
// Two induction variables are carried, an index for the trip count and a
// pointer stepping by 8, and the index is re-scaled at the hit -- so the
// index is what the source names and the pointer is strength reduction.
//
// Nothing in this body is relocated.

#include "types.h"

struct PairedObj
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ void* result;
};
ASSERT_OFFSET(PairedObj, result, 0x08);

struct KeyPair
{
    /* 0x00 */ PairedObj* obj;
    /* 0x04 */ void*      key;
};
ASSERT_SIZE(KeyPair, 8);

struct PairTable
{
    /* 0x00 */ char     unk0000[0x60];
    /* 0x60 */ KeyPair* pairs;
    /* 0x64 */ s32      count;
};
ASSERT_OFFSET(PairTable, pairs, 0x60);
ASSERT_OFFSET(PairTable, count, 0x64);

void* FindPaired(PairTable* t, void* owner)
{
    void* key = (char*)owner + 16;

    for (int i = 0; i < t->count; i++)
        if (t->pairs[i].key == key)
            return t->pairs[i].obj->result;

    return 0;
}
