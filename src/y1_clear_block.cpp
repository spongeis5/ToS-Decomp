#include "types.h"

// sub_82157C08 -- look the object's slot index up, run virtual slot 79 on
// it, then zero that slot's 516-byte block in the caller's array. 80 B.
// Bridge between 82157C00 and m_clear_rec516 (82157C58).
//
//      bl __savegprlr_29 ; mr r31,r3 ; mr r3,r4 ; mr r30,r4
//      bl 0x822509f0                     -> the index
//      lwz r11,0(r30) ; mr r29,r3 ; mr r3,r30
//      lwz r10,316(r11) ; mtctr ; bctrl  vtable slot 79
//      mulli r11,r29,516 ; li r5,516 ; li r4,0
//      add r3,r11,r31 ; bl 0x828a8c50    memset
//      b __restgprlr_29
//
// 516 is the same block size h_blk_ctor.cpp measured, and `mulli` is the
// only way to apply it -- it is not a shift.
//
// The index is computed BEFORE the virtual call and kept in a non-volatile
// across it, so the two are separate statements in that order and the index
// is a named local.

struct Slot;

struct SlotVT
{
    void* slot[79];
    void (*step)(Slot*);
};

struct Slot
{
    /* 0x00 */ SlotVT* vt;
};
ASSERT_OFFSET(Slot, vt, 0x00);

s32 SlotIndexOf(Slot* s);      /* sub_822509F0 */

extern "C" void* __cdecl memset(void* d, int c, size_t n);

void ClearSlotBlock(char* blocks, Slot* s)
{
    s32 i = SlotIndexOf(s);

    s->vt->step(s);

    memset(blocks + i * 516, 0, 516);
}
