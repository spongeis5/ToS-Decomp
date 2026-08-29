#include "types.h"

// sub_82600A60 -- initialise an empty circular list. 36 B, 16 callers.
//
//      addi    r11,r3,4         &s->next -- the sentinel node IS the head
//      stw     r4,12(r3)
//      li      r10,0
//      stw     r4,16(r3)
//      stw     r11,8(r3)
//      stw     r10,0(r3)
//      stw     r11,4(r3)
//      stw     r5,20(r3)
//      blr
//
// `addi r11,r3,4` computed once and stored into BOTH +4 and +8 is the empty
// circular list: next and prev both point at the head's own node. The two
// argument stores at +0x0C and +0x10 take the same value.
//
// Store order is source order -- 12, 16, 8, 0, 4, 20 -- and it is neither
// field order nor address order.
struct ListHead
{
    s32   count;
    void* next;
    void* prev;
    void* ownerA;
    void* ownerB;
    void* context;
};
ASSERT_OFFSET(ListHead, next, 0x04);
ASSERT_OFFSET(ListHead, prev, 0x08);
ASSERT_OFFSET(ListHead, context, 0x14);

void InitList(ListHead* h, void* owner, void* context)
{
    h->ownerA  = owner;
    h->ownerB  = owner;
    h->prev    = &h->next;
    h->count   = 0;
    h->next    = &h->next;
    h->context = context;
}
