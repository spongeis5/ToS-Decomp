// sub_821F6C40 -- release an object's slot: clear a flag byte, drop the slot
// table entry, push the id onto a global ring, and zero the id. 156 B,
// 5 callers.
//
//      lwz  r10,132(r3) ; li r7,0 ; stb r7,131(r3)
//      cmpwi cr6,r10,0 ; beqlr                     SIGNED -> the id is s32
//      lis/lis ; rlwinm r8,r10,2,0,29 ; addi r11,r11,31984  = 829A7CF0
//      lwz  r10,29888(r9)                          = *(u32**)829A74C0
//      stwx r7,r8,r10                              slots[id] = 0
//      lwz  r10,0(r11) ; cmplwi ; lwz r9,132(r3) ; beq-
//      cmplwi cr6,r9,0 ; beq-                      UNSIGNED -> a u32 here
//      ...
//
// The `stb` is emitted BEFORE the `beqlr`, so it is written before the guard:
// a store cannot be hoisted above a branch, and the load of 132(r3) can.
//
// 132(r3) is reloaded after `stwx` because that store is through a pointer
// the compiler cannot prove disjoint from the object -- genuine aliasing, so
// the field is spelled out at both uses rather than held in a local.
//
// The signedness split -- `cmpwi` on the first test and `cmplwi` on the
// second -- is the tell that the value crosses into an unsigned parameter,
// so the ring push is an inlined helper taking a u32.

#include "types.h"

struct Ring
{
    /* 0x00 */ u32* buf;
    /* 0x04 */ u32  write;
    /* 0x08 */ u32  read;
    /* 0x0C */ u32  f12;
    /* 0x10 */ u32  cap;
};

extern Ring  g_ring;      /* 829A7CF0 */
extern u32*  g_slots;     /* 829A74C0 */

struct Holder
{
    /* 0x000 */ char unk0000[131];
    /* 0x083 */ u8   busy;
    /* 0x084 */ s32  id;
};
ASSERT_OFFSET(Holder, busy, 131);
ASSERT_OFFSET(Holder, id, 132);

static void RingPush(u32 v)
{
    if (g_ring.buf != 0 && v != 0)
    {
        g_ring.buf[g_ring.write] = v;
        g_ring.write = g_ring.write + 1;
        if (g_ring.write >= g_ring.cap)
            g_ring.write = 0;
        if (g_ring.write == g_ring.read)
        {
            g_ring.f12 = 0;
            g_ring.read = 0;
            g_ring.write = 0;
        }
    }
}

void ReleaseSlot(Holder* h)
{
    h->busy = 0;
    if (h->id == 0)
        return;

    g_slots[h->id] = 0;
    RingPush((u32)h->id);
    h->id = 0;
}
