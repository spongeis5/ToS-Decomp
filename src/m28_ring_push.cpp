// sub_827DA5E0 -- take the next slot of a 100-entry ring, evicting the oldest
// when it is full. 92 bytes, 4 callers.
//
//      lwz     r11,2004(r3)      count
//      cmplwi  cr6,r11,100 ; bne- cr6,<join>
//      lwz     r11,2000(r3)      head
//      li      r10,99
//      addi    r11,r11,1
//      stw     r10,2004(r3)      count = 99      <- stored FIRST
//      stw     r11,2000(r3)      head  = head+1
//      cmplwi  cr6,r11,100 ; bne- cr6,<join>
//      li      r11,0 ; stw r11,2000(r3)
//  join:
//      lwz     r10,2004(r3)      count           <- reloaded across the join
//      lwz     r11,2000(r3)      head
//      add     r11,r11,r10
//      cmplwi  cr6,r11,100 ; blt- ; addi r11,r11,-100
//      addi    r10,r10,1
//      mulli   r11,r11,20
//      stw     r10,2004(r3)
//      add     r3,r11,r3
//      blr
//
// 100 * 20 == 2000, which is exactly where the two counters sit, so the ring
// buffer IS the object's first member and `add r3,r11,r3` needs no offset.
//
// `add r11,r11,r10` puts head in rA. Per the add-operand rule rA holds the
// operand whose SOURCE read comes later, and the loads bear that out -- count
// is issued first -- so the sum is written `count + head`, not `head + count`.
//
// Every compare is `cmplwi`, so both counters are unsigned, and the wrap test
// is `blt-` after one: an unsigned `i >= 100`.
//
// `mulli r11,r11,20` by a small constant is the LOUD /Os signature: 20 is
// (i + i*4) * 4, three instructions MSVC is happy to spend at /O2.

#include "types.h"

struct RingSlot
{
    /* 0x00 */ u8 unk0000[20];
};

ASSERT_SIZE(RingSlot, 20);

struct Ring
{
    /* 0x0000 */ RingSlot slots[100];
    /* 0x07D0 */ u32      head;
    /* 0x07D4 */ u32      count;
};

ASSERT_OFFSET(Ring, head, 0x7D0);
ASSERT_OFFSET(Ring, count, 0x7D4);

RingSlot* RingPush(Ring* r)
{
    if (r->count == 100)
    {
        r->count = 99;
        r->head = r->head + 1;
        if (r->head == 100)
            r->head = 0;
    }

    u32 i = r->count + r->head;
    if (i >= 100)
        i -= 100;

    r->count = r->count + 1;
    return &r->slots[i];
}
