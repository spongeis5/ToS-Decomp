// sub_826DD4A0 -- find an item in the owner's slot table, clear that slot and
// the item's back pointer, then set two 2-bit flags. 96 B, 5 callers.
//
//      lwz    r9,60(r3)          count, loaded ONCE and held for the loop
//      li     r8,0 ; mr r11,r8   the shared zero; r11 is the index
//      cmpwi  cr6,r9,0 ; ble-    -> the -1
//      lwz    r10,56(r3)         items, held as an induction pointer
//   L: lwz    r7,0(r10) ; cmplw cr6,r7,r4 ; beq- found
//      addi   r11,r11,1 ; addi r10,r10,4 ; cmpw cr6,r11,r9 ; blt+ L
//      li     r11,-1
//   F: lwz    r10,56(r3)         RELOADED after the inlined search
//      rlwinm r9,r11,2,0,29 ; stwx r8,r9,r10
//      stw    r8,12(r4)
//      lbz    r8,37(r3) ; clrlwi r7,r8,26 ; rlwinm r7,r7,0,30,27
//      ori    r6,r7,68 ; stb r6,37(r3)
//
// The `li r11,-1` reached by falling out of the loop, with the found case
// branching PAST it, is a search helper returning -1 -- and the caller then
// indexes with that result unchecked, which is what the reload of 56(r3)
// after the loop shows: the helper is inlined but the outer expression reads
// the field again.
//
// THE TWO MASKS ARE TWO BITFIELD ASSIGNMENTS, not one AND. `& 0x3F` then
// `& ~0x0C` then `| 0x44` is `x & 0x33 | 0x44`, and 0x33 is not a contiguous
// mask, so MSVC cannot fold the two `rlwinm`s -- while `| 0x40` and `| 0x04`
// ARE foldable and come out as one `ori 68`. Per MATCHED.md, MSVC allocates
// bitfields MSB-first, so the field cleared by `& 0x3F` (bits 7:6) is
// declared FIRST and the one cleared by `& ~0x0C` (bits 3:2) is third; the
// order of the two masks is the order of the two assignments.

#include "types.h"

struct SlotItem
{
    /* 0x00 */ u8    unk0000[0x0C];
    /* 0x0C */ void* owner;
};
ASSERT_OFFSET(SlotItem, owner, 0x0C);

struct SlotOwner
{
    /* 0x00 */ u8        unk0000[0x25];
    /* 0x25 */ u8        state : 2;
    /*      */ u8        pad25 : 2;
    /*      */ u8        mode  : 2;
    /*      */ u8        pad25b : 2;
    /* 0x26 */ u8        unk0026[0x12];
    /* 0x38 */ SlotItem** items;
    /* 0x3C */ s32       count;
};
ASSERT_OFFSET(SlotOwner, items, 0x38);
ASSERT_OFFSET(SlotOwner, count, 0x3C);

static s32 IndexOf(SlotOwner* o, SlotItem* it)
{
    s32 n = o->count;

    for (s32 i = 0; i < n; i++)
        if (o->items[i] == it)
            return i;
    return -1;
}

void SlotRemove(SlotOwner* o, SlotItem* it)
{
    o->items[IndexOf(o, it) & 0x3FFFFFFF] = 0;
    it->owner = 0;
    o->state = 1;
    o->mode = 1;
}
