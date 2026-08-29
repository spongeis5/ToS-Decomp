// sub_825FAB10 -- pick a group out of a two-level table, find the first entry
// carrying every bit of a mask, and turn one of its pointers into an index.
// 136 B, 5 callers.
//
//   rlwinm r11,r4,3,0,28 ; subf r11,r4,r11    a*8 - a  =  a*7
//   add    r11,r11,r5    ; addi r6,r11,3
//   rlwinm r5,r6,2,0,29  ; lwzx r11,r5,r9     table[a*7 + b + 3]
//
// `lwzx` with the INDEX in rA is the shape a subscript of a pointer field
// gives when the pointed-at array starts at offset 0 (MATCHED.md's operand
// table), and `lwz r9,76(r3)` loads that pointer, so the `+3` is an index and
// not a folded byte offset -- the folded-offset lever only applies to arrays
// laid out INSIDE the object.
//
//   add r11,r11,r10   with r10 = c*8      ->  &row[c], Group is 8 bytes
//   rlwinm r9,r10,1 ; add r4,r10,r9 ; rlwinm r10,r4,2    ->  count*12
//
// is the documented `i * 12` idiom, so the entries are 12 bytes.
//
//   li r11,0            reached by falling out of the loop
//   lwz r11,0(r11)      then dereferenced UNCONDITIONALLY
//
// The miss path loads through a null pointer. That is not a misreading -- the
// `li` is the fall-through of the loop's bottom test and the `lwz` is the
// join both paths reach -- it is a two-return helper inlined into an
// expression whose caller never checks, which is why it is written that way
// here rather than being "fixed".
//
//   and r6,r9,r7 ; cmpw cr6,r6,r7      (e->flags & mask) == mask, SIGNED
//   subf r6,r9,r7 ; li r8,40 ; divw    a POINTER DIFFERENCE over a 40-byte
//                                      element, not a written division
//   clrlwi r3,r5,24                    the return is `u8`
//
// The trailing mask is a real truncation of an index, not the `bool` tell --
// that lever is about a value already 0 or 1, which this is not.

#include "types.h"

struct Object
{
    /* 0x00 */ u8 unk0000[40];
};

ASSERT_SIZE(Object, 40);

struct Entry
{
    /* 0x00 */ Object** list;
    /* 0x04 */ int      unk0004;
    /* 0x08 */ int      flags;
};

ASSERT_SIZE(Entry, 12);

struct Group
{
    /* 0x00 */ Entry* first;
    /* 0x04 */ int    count;
};

ASSERT_SIZE(Group, 8);

struct Owner
{
    /* 0x00 */ u8      unk0000[0x20];
    /* 0x20 */ Object* objects;
    /* 0x24 */ u8      unk0024[0x28];
    /* 0x4C */ Group** table;
};

ASSERT_OFFSET(Owner, objects, 0x20);
ASSERT_OFFSET(Owner, table, 0x4C);

static Entry* FindFlagged(Entry* p, Entry* end, int mask)
{
    while (p != end)
    {
        if ((p->flags & mask) == mask)
            return p;
        p = p + 1;
    }
    return 0;
}

u8 SlotIndex(Owner* o, int a, int b, int c, int mask, int k)
{
    Group* g = &o->table[a * 7 + b + 3][c];
    Entry* e = FindFlagged(g->first, g->first + g->count, mask);

    return (u8)(e->list[k] - o->objects);
}
