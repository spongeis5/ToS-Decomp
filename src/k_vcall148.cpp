#include "types.h"

// sub_827C5180 -- virtual call through a member object. 20 B, 9 callers.
//   lwz r3,116(r3) ; lwz r11,0(r3) ; lwz r11,148(r11) ; mtctr r11 ; bctr
//
// The twin of sub_827C5198 (src/vcall116.cpp) sixteen bytes further on: same
// member at +0x74, same shape, a different vtable slot -- 148/4 = 37 rather
// than 64/4 = 16. Two adjacent COMDATs of the same idiom, so if the flag
// story holds they must agree, and they do: both need /O2 /Os. At plain /O2
// the vtable slot load goes to a fresh r10 where the target coalesces it onto
// r11, which is the register-coalescing signature and not a source shape.
struct Target148;
struct VT148 { void* (*slot[38])(Target148*); };
struct Target148 { VT148* vt; };
ASSERT_OFFSET(Target148, vt, 0x00);

struct Owner148
{
    char unk0000[0x74];
    Target148* member;
    void* Call();
};
ASSERT_OFFSET(Owner148, member, 0x74);

void* Owner148::Call()
{
    Target148* t = member;
    return t->vt->slot[37](t);
}
