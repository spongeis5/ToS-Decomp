// sub_826779C8 -- find an item in one of the owner's five slot tables and
// zero that slot. 72 bytes of the 360-byte .pdata row, 2 callers.
//
// THE ROW COVERS FIVE FUNCTIONS, not one. 826779C8, 82677A10, 82677A58,
// 82677AA0 and 82677AE8 are five byte-identical bodies differing only in the
// pair of field offsets they read, each ending in its own `blr`, and one
// unwind record spans the run because they are all frameless. Only the first
// is a known function start, so only the first can be compared; match.py's
// `can_shrink` establishes the boundary from the image itself -- our code
// ends in an unconditional terminator, the retail word there is one too, and
// no branch in the compared range reaches past it.
//
//      lwz    r9,340(r3)         count
//      li     r11,0              i
//      cmpwi  cr6,r9,0 ; ble-    -> the -1
//      lwz    r10,336(r3)        items, held as an induction pointer
//   L: lwz    r8,0(r10) ; cmplw cr6,r8,r4 ; beq- found
//      addi   r11,r11,1 ; addi r10,r10,4 ; cmpw cr6,r11,r9 ; blt+ L
//      li     r11,-1
//   F: lwz    r10,336(r3)        RELOADED after the inlined search
//      rlwinm r9,r11,2,0,29 ; li r8,0 ; stwx r8,r9,r10
//
// This is src/k4_slot_remove.cpp's shape exactly, including the reload of
// the items pointer after the inlined search and the `stwx` with the INDEX
// in rA -- which per MATCHED.md is the AND-mask on the subscript, absorbed
// into the `rlwinm` the `* 4` already needed and therefore invisible except
// as the operand order. The index is -1 when the item is absent and the
// store happens anyway, so the mask changes nothing the function computes.
//
// The two zeros are materialised separately here (`li r11,0` for the index
// and `li r8,0` for the stored value) where k4 shares one, because k4 has a
// second store of the same zero and this has only one.

#include "types.h"

struct SlotItem;

struct FiveSlotOwner
{
    /* 0x000 */ u8         unk0000[0x150];
    /* 0x150 */ SlotItem** items;
    /* 0x154 */ s32        count;
};
ASSERT_OFFSET(FiveSlotOwner, items, 0x150);
ASSERT_OFFSET(FiveSlotOwner, count, 0x154);

static s32 IndexOf(FiveSlotOwner* o, SlotItem* it)
{
    s32 n = o->count;

    for (s32 i = 0; i < n; i++)
        if (o->items[i] == it)
            return i;
    return -1;
}

void ClearSlot150(FiveSlotOwner* o, SlotItem* it)
{
    o->items[IndexOf(o, it) & 0x3FFFFFFF] = 0;
}
