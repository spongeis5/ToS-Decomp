// sub_8224B698 -- fetch a member pointer and upcast it. 24 B, 3 callers.
//
//      lwz    r11,72(r3)
//      cmplwi cr6,r11,0
//      addi   r3,r11,64
//      bnelr  cr6
//      li     r3,0
//
// The idiom-table BASE-CLASS UPCAST: `cmplwi ; addi ; bne- ; li 0`.  The
// null test guards only the +64 adjustment, because `static_cast<Base*>(p)`
// has to keep a null pointer null.  Nobody writes that by hand -- a
// hand-written `p ? &p->part : 0` is the same code but says the programmer
// distrusted the pointer, and here nothing dereferences it at all.
//
// Same shape as src/m_upcast_call.cpp with the base at +64 rather than +256,
// and with the adjusted pointer RETURNED instead of passed on.

#include "types.h"

struct ItemHead
{
    char unk0000[64];
};

struct ItemBody
{
    /* 0x00 */ void* first;
};

struct Item : ItemHead, ItemBody
{
};
ASSERT_OFFSET(Item, first, 64);

struct ItemOwner
{
    /* 0x00 */ char  unk0000[0x48];
    /* 0x48 */ Item* item;
};
ASSERT_OFFSET(ItemOwner, item, 0x48);

ItemBody* GetBody(ItemOwner* o)
{
    return static_cast<ItemBody*>(o->item);
}
