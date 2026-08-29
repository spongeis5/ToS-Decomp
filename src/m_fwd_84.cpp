#include "types.h"

// sub_82677058 -- forward through a field. 8 bytes, 14 callers.
//
//      lwz     r3,132(r3)
//      b       0x826E4B30
//
// The inventory records this row as 16 bytes because a SECOND 8-byte body
// sits immediately after it -- `lwz r3,132(r3) ; b 826E5030`, the same
// forward to a different handler. Nothing branches to that second body and
// no data word points at it, so neither the branch sweep nor the
// data-pointer scan sees it, and the row's size runs to the next start it
// does know. match.py's can_shrink() proves the row covers two functions and
// compares only this one.
//
// It sits 0x18 after ClearAndHandleOther in owner_clear.cpp, so this is very
// likely the same translation unit, but nothing here needs that to be true.
struct Held;
struct Owner84
{
    char   unk0000[132];
    Held*  held;
};
ASSERT_OFFSET(Owner84, held, 132);

int Consume(Held* h);

int ConsumeHeld(Owner84* o)
{
    return Consume(o->held);
}
