#include "types.h"

// sub_8261A3D8 -- the first body of a 56-byte inventory row that covers two
// functions (12 bytes here, then a pad and a 40-byte comparator). 3 callers.
//
//      lwz     r11,4(r3)
//      lwz     r3,12(r11)
//      blr
//
// can_shrink reconciles the row to our body's length, exactly as it did for
// 8215E5B0 (FINDINGS 7q).

struct Node12
{
    /* 0x0C */ char unk0000[12];
    /* 0x0C */ void* value;
};

ASSERT_OFFSET(Node12, value, 12);

struct Holder4
{
    /* 0x04 */ char     unk0000[4];
    /* 0x04 */ Node12* node;
};

ASSERT_OFFSET(Holder4, node, 4);

void* GetValue(Holder4* h)
{
    return h->node->value;
}
