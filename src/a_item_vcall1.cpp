// sub_8224E6F8 -- 40 bytes, 54 callers. Index a pointer array, then make a
// virtual call on the element forwarding the two remaining arguments.
//
//      lwz     r10,0(r3)        this->items
//      rlwinm  r9,r4,2,0,29     i * 4
//      mr      r11,r5           park arg3
//      mr      r5,r6            arg4 -> slot 3
//      mr      r4,r11           arg3 -> slot 2
//      lwzx    r3,r9,r10        items[i]
//      lwz     r8,0(r3)         vtable
//      lwz     r7,4(r8)         slot 4/4 = 1
//      mtctr   r7
//      bctr
//
// The three-instruction shuffle through r11 is the whole tell: r4 (the
// index) is still live when r5 has to move down into r4, so the compiler
// needs a scratch. That only happens when the index is consumed by the
// SUBSCRIPT and the object it selects becomes the call's `this`, i.e. the
// element drops out of the argument list entirely.
//
// Nothing here is relocated; all 10 words are compared.

#include "types.h"

struct Item;

struct ItemVT
{
    void* (*slot[2])(Item*, void*, void*);
};

struct Item
{
    /* 0x00 */ ItemVT* vt;
};

ASSERT_OFFSET(Item, vt, 0x00);

struct ItemArray
{
    /* 0x00 */ Item** items;
};

ASSERT_OFFSET(ItemArray, items, 0x00);

void* InvokeItem(ItemArray* a, int i, void* p, void* q)
{
    Item* it = a->items[i];
    return it->vt->slot[1](it, p, q);
}
