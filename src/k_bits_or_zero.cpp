#include "types.h"

// sub_82631D98 -- copy a two-bit field out of a linked object, or zero.
// 36 B of code (the .pdata row spans the frameless bodies after it).
// 10 callers.
//
//      lwz     r11,200(r4)
//      cmpwi   cr6,r11,0
//      bne-    cr6,have
//      stb     r11,0(r3)       <- stores the register KNOWN to be zero
//      blr
// have:lbz     r11,38(r11)
//      rlwinm  r10,r11,28,30,31    (x >> 4) & 3
//      stb     r10,0(r3)
//      blr
//
// The null path stores r11 itself rather than materialising a zero, so the
// stored value is literally the pointer's zero. `bne-` jumping away to the
// interesting path means the null case is the fall-through and is written
// first.
struct Node
{
    /* 0x00 */ char unk0000[38];
    /* 0x26 */ u8   flags;
};
ASSERT_OFFSET(Node, flags, 38);

struct Source
{
    /* 0x00 */ char unk0000[200];
    /* 0xC8 */ s32  node;
};
ASSERT_OFFSET(Source, node, 200);

struct Holder
{
    /* 0x00 */ u8 value;

    void Set(Source* s);
};

void Holder::Set(Source* s)
{
    s32 n = s->node;
    if (n == 0)
    {
        value = 0;
        return;
    }
    value = (u8)((((Node*)n)->flags >> 4) & 3);
}
