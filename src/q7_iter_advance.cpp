#include "types.h"

// sub_8287E3D0 -- advance a hash iterator past the free slots. 112 B,
// 8 callers.  Sits IMMEDIATELY before sub_8287E440 (c_iter_equal.cpp) and
// uses the identical types: owner at +0 / index at +4, bucket at +0,
// bucket->last at +4.  That neighbour is /O2 /Os.
//
//      lwz     r11,0(r3)       c = it->owner
//      lwz     r10,4(r3)       i = it->index
//      lwz     r9,0(r11) ; lwz r9,4(r9)
//      cmpw    cr6,r10,r9      SIGNED
//      bgtlr   cr6
//      addi    r10,r10,1
//      stw     r10,4(r3)
//      lwz     r9,0(r11) ; lwz r9,4(r9)
//      cmplw   cr6,r10,r9      UNSIGNED -- a signedness split on the SAME pair
//      bgtlr   cr6
//  L:  lwz     r10,4(r3)
//      lwz     r8,0(r11)       c->bucket, from the CACHED owner
//      rlwinm  r9,r10,4,0,27
//      add     r9,r9,r8
//      lwz     r9,8(r9)        bucket->entries[i].tag, entries at +8, stride 16
//      cmpwi   cr6,r9,-2
//      bnelr   cr6
//      addi    r10,r10,1
//      stw     r10,4(r3)
//      lwz     r9,4(r3)
//      lwz     r10,0(r3)       it->owner RELOADED
//      lwz     r10,0(r10) ; lwz r10,4(r10)
//      cmplw   cr6,r9,r10
//      ble+    cr6,L
//      blr
//
// The owner is loaded once and kept in r11 across the two guards and the loop
// BODY, but the back-edge condition loads it again from `it` -- and nothing
// between them stores anywhere that could alias a field of `it`.  Per the
// CSE-defeat lever that means the two reads are spelled DIFFERENTLY: a local
// for the body, the full `it->owner->bucket->last` chain for the condition.
//
// `add r9,r9,r8` puts the scaled index in rA and the base in rB, which is the
// `bucket->entries[index]` read order -- base first, index later.
struct IterEntry
{
    /* 0x00 */ s32  tag;
    /* 0x04 */ char unk0004[0x0C];
};
ASSERT_SIZE(IterEntry, 16);

struct IterBucket
{
    /* 0x00 */ u32       unk0000;
    /* 0x04 */ int       last;
    /* 0x08 */ IterEntry entries[1];
};
ASSERT_OFFSET(IterBucket, last,    0x04);
ASSERT_OFFSET(IterBucket, entries, 0x08);

struct IterOwner
{
    IterBucket* bucket;
};

struct Iterator
{
    IterOwner* owner;
    int        index;
};
ASSERT_OFFSET(Iterator, index, 0x04);

void IterAdvance(Iterator* it)
{
    IterOwner* c = it->owner;

    if (it->index > c->bucket->last)
        return;

    it->index = it->index + 1;

    while ((u32)it->index <= (u32)it->owner->bucket->last)
    {
        if (c->bucket->entries[it->index].tag != -2)
            return;
        it->index = it->index + 1;
    }
}
