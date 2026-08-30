#include "types.h"

// sub_827EE530 -- guarded fallback to a virtual call on another object.
// 40 B, bridge between 827EE528 and 827EE558.
//
//   p = o->f428
//   if (p) return p->f28
//   r3 = o->f36; fn = vtable[19]; return fn(r3)      (lwz 0(r3) ; lwz
//                                                     76(r11) ; mtctr ; bctr)

struct WithVt
{
    void* slots[19];
    void* slot19;
};

struct Inner
{
    /* 0x1C */ char  unk0000[28];
    /* 0x1C */ void* f28;
};

struct HolderV
{
    /* 0x00 */ char  unk0000[36];
    /* 0x24 */ Inner* f36;
    /* 0x28 */ char  unk0028[388];
    /* 0x1AC */ Inner* f428;
};

ASSERT_OFFSET(HolderV, f36, 36);
ASSERT_OFFSET(HolderV, f428, 428);
ASSERT_OFFSET(Inner, f28, 28);

void* GetOrCall(HolderV* o)
{
    if (o->f428)
        return o->f428->f28;
    WithVt* w = (WithVt*)o->f36;
    return ((void* (*)(WithVt*))w->slot19)(w);
}

// NEAR-MISS. vtable slot 19 tail call; declared fn-ptr form differs.
