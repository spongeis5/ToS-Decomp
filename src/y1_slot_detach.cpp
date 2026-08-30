#include "types.h"

// sub_826DD438 -- swap-remove an item from its owner's pointer table by the
// item's own stored index, then invalidate the item. 104 B.
//
// Bridge between Acc_826DD430 and SlotRemove (826DD4A0), and it is the same
// class family: the two-bit flag byte at 0x25 is exactly the one
// k4_slot_remove.cpp already decoded, and `rlwimi r10,r6,6,0,25` is the
// single-assignment form of it -- odd part 1 in r6, the power of two folded
// into the rotate, so the byte becomes (byte & 0x3F) | 0x40, i.e. the
// FIRST-declared 2-bit field set to 1 (MSVC allocates bitfields MSB-first).
//
//      lwz    r10,76(r3) ; lwz r11,72(r3)   count, items
//      rlwinm r10,r10,2,0,29 ; add r5,r10,r11 ; lwz r5,-4(r5)
//                                            items[count-1], biased
//      lhz    r7,164(r4) ; rotlwi r7,r7,2   the u16 index * 4, NO mask
//                                            needed because it is 16 bits
//      stwx   r5,r7,r11                     index in rA
//      addi   r10,r3,72                     DEAD -- never read
//      lwz    r11,72(r3) ; lhz r10,164(r4)  both RELOADED across the store
//      lwzx   r5,r7,r11 ; sth r10,164(r5)
//
// The dead `addi r10,r3,72` is the sub_82703E28 / sub_82164040 fingerprint:
// an inlined helper whose pointer argument is materialised once and then
// folded away at every use. It needs TWO nesting levels -- the flat body is
// 100 bytes with no leftover -- so the table is a sub-object at +72 and the
// swap is a helper on it that calls another helper.

struct DItem
{
    /* 0x00 */ u8    unk0000[0xA4];
    /* 0xA4 */ u16   index;
    /* 0xA6 */ u8    unk00A6[0x22];
    /* 0xC8 */ void* owner;
};
ASSERT_OFFSET(DItem, index, 0xA4);
ASSERT_OFFSET(DItem, owner, 0xC8);

struct DTable
{
    /* 0x00 */ DItem** items;
    /* 0x04 */ s32     count;
};
ASSERT_OFFSET(DTable, count, 0x04);

struct DOwner
{
    /* 0x00 */ u8     unk0000[0x25];
    /* 0x25 */ u8     state : 2;
    /*      */ u8     rest25 : 6;
    /* 0x26 */ u8     unk0026[0x22];
    /* 0x48 */ DTable tab;
};
ASSERT_OFFSET(DOwner, tab, 0x48);

static DItem* Back(DTable* t)
{
    return t->items[t->count - 1];
}

static void SwapRemove(DTable* t, DItem* it)
{
    t->items[it->index] = Back(t);
    t->items[it->index]->index = it->index;
    t->count = t->count - 1;
}

void SlotDetach(DOwner* o, DItem* it)
{
    SwapRemove(&o->tab, it);
    it->owner = 0;
    it->index = (u16)-1;
    o->state = 1;
}
