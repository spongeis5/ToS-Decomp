// sub_82606158 -- first word of the i'th 8-byte entry of a table reached
// through a global pointer. 20 B, 3 callers.
//
//      lis    r9,-32093
//      rlwinm r10,r4,3,0,28      i * 8
//      lwz    r11,18548(r9)      the TABLE POINTER, at 82A34874
//      lwzx   r3,r10,r11
//
// Two things the register discipline settles:
//
//  * r3 arrives and is overwritten without being read, while the index is in
//    r4 -- the first parameter is unused, which is a member function whose
//    body only touches its argument (the same shape as sub_82790F80).
//  * the global is READ, not addressed: `lis` + `lwz` with the low half as
//    the displacement is a load of the variable, and the value loaded is
//    then used as a base.  So the table is behind a pointer, not a global
//    array -- had it been an array the base would be an addi.
//
// `rlwinm rD,rS,3,0,28` is `i * 8`, so the element is 8 bytes and the field
// is at its offset 0 -- which is also why the index lands in rA of the
// `lwzx`.

#include "types.h"

struct Slot8
{
    /* 0x00 */ void* first;
    /* 0x04 */ void* second;
};
ASSERT_SIZE(Slot8, 8);

extern Slot8* g_slot_table;      /* 82A34874 */

struct SlotUser
{
    void* At(u32 i);
};

void* SlotUser::At(u32 i)
{
    return g_slot_table[i].first;
}
