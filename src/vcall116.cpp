#include "types.h"

// sub_827C5198 -- virtual call through a member object. 20 B, 48 callers.
//   lwz r3,116(r3) ; lwz r11,0(r3) ; lwz r11,64(r11) ; mtctr r11 ; bctr
//
// Seven free-function shapes all gave 3 of 5, with the vtable slot in r10
// where the target reuses r11. The member form is what fixed the same
// transposition in sub_826C0FC8.
struct Target116;
struct VT116 { void* (*slot[17])(Target116*); };
struct Target116 { VT116* vt; };
ASSERT_OFFSET(Target116, vt, 0x00);

struct Owner116
{
    char unk0000[0x74];
    Target116* member;
    void* Call();
};
ASSERT_OFFSET(Owner116, member, 0x74);

void* Owner116::Call()
{
    Target116* t = member;
    return t->vt->slot[16](t);
}
