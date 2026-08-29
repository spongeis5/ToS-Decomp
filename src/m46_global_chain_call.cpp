// sub_8225D080 -- read a pointer out of a global, dereference it once, and
// tail-call with the result. 20 bytes, 3 callers.
//
//      lis     r11,-32101
//      addi    r10,r11,-19728      -> 829AB2F0, the global's address
//      lwz     r11,1084(r10)
//      lwz     r3,0(r11)
//      b       0x822D1508
//
// The address is materialised into a register and the field offset stays in
// the load, which per src/global_field.cpp is a MEMBER of a global object
// rather than a pointer variable -- a pointer variable folds its low half
// into the load itself and needs no `addi`.
//
// The function's own r3 is overwritten before the branch, so it takes no
// arguments of its own; sub_822D1508 reads only r3.
//
// 2 of 5 words are relocated.

#include "types.h"

struct Slot;

struct Registry5D
{
    /* 0x000 */ u8     unk0000[1084];
    /* 0x43C */ Slot** current;
};

ASSERT_OFFSET(Registry5D, current, 1084);

extern Registry5D g_registry5D;

int LookupSlot(Slot* s);

int LookupCurrent()
{
    return LookupSlot(*g_registry5D.current);
}
