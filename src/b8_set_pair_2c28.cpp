#include "types.h"

// sub_8214C788 -- guarded store of two pointer fields. 20 B, 5 callers.
//
//      cmplwi  cr6,r3,0
//      beqlr   cr6
//      stw     r4,44(r3)
//      stw     r5,40(r3)
//      blr
//
// Store order is source order, so 0x2C is written BEFORE 0x28 even though
// that is not address order. Immediate neighbour of sub_8214C778, which is
// the same guard writing one field.

struct Holder2C
{
    /* 0x00 */ char  unk0000[0x28];
    /* 0x28 */ void* field28;
    /* 0x2C */ void* field2C;
};
ASSERT_OFFSET(Holder2C, field28, 0x28);
ASSERT_OFFSET(Holder2C, field2C, 0x2C);

void SetPair2C28(Holder2C* h, void* a, void* b)
{
    if (h != 0)
    {
        h->field2C = a;
        h->field28 = b;
    }
}
