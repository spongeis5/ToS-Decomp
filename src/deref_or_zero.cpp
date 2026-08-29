#include "types.h"

// sub_822D0BE8 -- guarded dereference. 32 B, 5 callers.
//   lwz r11,68(r3) ; cmplwi cr6,r11,0 ; ble- cr6,0x822D0C00
//   lwz r11,72(r3) ; lwz r3,0(r11) ; blr
//   li r3,0 ; blr
//
// cmplwi with ble is an UNSIGNED <= 0, true only at zero, so the count is
// unsigned. The branch jumps AWAY to the zero return, so the dereference is
// the fall-through and must be written first.
struct Counted { char unk0000[0x44]; u32 count; void** items; };
ASSERT_OFFSET(Counted, count, 0x44);
ASSERT_OFFSET(Counted, items, 0x48);

void* FirstOrNull(Counted* c)
{
    if (c->count)
        return c->items[0];
    return 0;
}
