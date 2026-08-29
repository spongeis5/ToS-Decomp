#include "types.h"

// sub_8261B2F8 -- bit test picks between a virtual call and a field walk.
// 52 B, 27 callers.
//   lbz    r11,17(r3)
//   rlwinm r10,r11,0,30,30      mask 0x2
//   cmplwi cr6,r10,0
//   beq-   cr6,0x8261b31c
//   lwz    r3,20(r3) ; lwz r11,0(r3) ; lwz r10,8(r11)
//   mtctr  r10 ; bctr           virtual slot 8/4 = 2, tail call
// 0x8261b31c:
//   lwz    r11,20(r3) ; lwz r10,4(r11) ; lwz r3,12(r10) ; blr
//
// `beq-` jumps AWAY to the field walk, so the virtual call is the
// fall-through and is written first.
//
// The object at +20 has its vtable at +0 and an ordinary pointer member at
// +4, which is exactly a class with virtual functions and one data member.

struct Payload
{
    char unk0000[0x0C];
    void* value;
};
ASSERT_OFFSET(Payload, value, 0x0C);

struct Target
{
    Payload* payload;             /* +4, after the vtable pointer */
    virtual void Slot0();
    virtual void Slot1();
    virtual void* Slot2();
};
ASSERT_OFFSET(Target, payload, 0x04);

struct Owner
{
    char    unk0000[0x11];
    u8      flags;                /* +0x11 */
    char    unk0012[0x02];
    Target* target;               /* +0x14 */
};
ASSERT_OFFSET(Owner, flags,  0x11);
ASSERT_OFFSET(Owner, target, 0x14);

void* GetValue(Owner* o)
{
    if (o->flags & 2)
        return o->target->Slot2();
    return o->target->payload->value;
}
