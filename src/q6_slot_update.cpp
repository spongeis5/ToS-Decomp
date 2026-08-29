#include "types.h"

// sub_8215C9C0 -- scan a table of 16-byte slots, and for every slot whose
// key (indirected through a side array) equals the argument, write two
// fields. 96 B, 8 callers.
//
//      lwz     r11,16(r3)      n = t->count
//      li      r9,0            i = 0
//      cmplwi  cr6,r11,0
//      blelr   cr6             UNSIGNED `0 < n` rotation guard
//      li      r11,0           off = 0
//  L:  lwz     r10,24(r3)      t->slots     (reloaded every iteration)
//      lwz     r8,8(r3)        t->keys      (reloaded every iteration)
//      lhzx    r7,r10,r11      slots[i].index
//      rotlwi  r10,r7,2
//      lwzx    r8,r10,r8       keys[index]
//      cmplw   cr6,r8,r4
//      bne-    cr6,skip
//      lwz     r10,24(r3)
//      add     r10,r10,r11
//      stw     r5,4(r10)       slots[i].a = a
//      lwz     r10,24(r3)
//      add     r8,r10,r11
//      stw     r6,8(r8)        slots[i].b = b
// skip:lwz     r10,16(r3)      t->count     (reloaded for the back edge)
//      addi    r9,r9,1
//      addi    r11,r11,16
//      cmplw   cr6,r9,r10
//      blt+    cr6,L
//      blr
//
// Every field is RELOADED: the two `stw`s through `t->slots` may alias any of
// them, so `slots`, `keys` and `count` are all re-read after them.  That is
// what spelling `t->slots[i]` out at each of its three uses gives; naming it
// in a local would hold it in a register and lose all four reloads.
//
// `cmplwi`/`blelr` rather than `cmplwi`/`beqlr` is the unsigned `0 < n` form
// -- an UNSIGNED count, matching the `cmplw` on the back edge.  The reload in
// the loop CONDITION is the normal shape (sub_825FEF00), not the CSE-defeat
// lever.
//
// Stride 16 with a u16 at element offset 0, and `rotlwi ...,2` on it: the
// side array at +8 is indexed by that u16 and holds 4-byte keys.
struct Slot
{
    /* 0x00 */ u16 index;
    /* 0x02 */ u16 unk0002;
    /* 0x04 */ u32 a;
    /* 0x08 */ u32 b;
    /* 0x0C */ u32 unk000C;
};
ASSERT_SIZE(Slot, 16);

struct SlotTable
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ u32*  keys;
    /* 0x0C */ char  unk000C[0x04];
    /* 0x10 */ u32   count;
    /* 0x14 */ char  unk0014[0x04];
    /* 0x18 */ Slot* slots;
};
ASSERT_OFFSET(SlotTable, keys,  0x08);
ASSERT_OFFSET(SlotTable, count, 0x10);
ASSERT_OFFSET(SlotTable, slots, 0x18);

void SetForKey(SlotTable* t, u32 key, u32 a, u32 b)
{
    for (u32 i = 0; i < t->count; i++)
    {
        if (t->keys[t->slots[i].index] == key)
        {
            t->slots[i].a = a;
            t->slots[i].b = b;
        }
    }
}
