#include "types.h"

// sub_826378D8 -- invalidate every key in an 8-byte-stride table and clear
// every flag but the top one. 56 B, 7 callers.
//
//      lwz     r11,8(r3)       n = t->last
//      addic.  r10,r11,1       trip = n + 1, sets CR0
//      ble-    done            n < 0: no iterations
//      mtctr   r10
//      li      r11,0           byte offset, strength-reduced
//      li      r10,-1
//  L:  lwz     r9,0(r3)        t->slots RELOADED every iteration
//      stwx    r10,r11,r9      slots[i].key = -1
//      addi    r11,r11,8       stride 8
//      bdnz+   L
// done:lwz     r11,4(r3)
//      rlwinm  r10,r11,0,0,0   & 0x80000000
//      stw     r10,4(r3)
//
// `mtctr` with the trip count computed up front says the bound is a LOCAL:
// a `for (i = 0; i <= t->last; ++i)` would have to re-read the field, which
// is exactly what the base pointer does. So `last` is read once into a local
// and `slots` is respelled through `t->` at its single use, where the store
// can alias the object and kills the load every time round.
//
// THE TRIP COUNT IS THE LOCAL, not the bound. Written `for (i = 0; i <= n;)`
// with `n = t->last`, MSVC emits `cmpwi cr6,r11,0 ; blt-` for the guard and a
// SEPARATE `addi r10,r11,1` for the count -- 60 bytes, 2 of 14 words, every
// later word displaced by one. Naming `t->last + 1` itself lets the add and
// the test against zero fuse into the one `addic.`, which is the same fusion
// src/f_vec_compact.cpp gets for `v->count - 1`.
//
// `rlwinm rX,rY,0,0,0` keeps bit 0 only -- the 0x80000000 mask, a flag test
// written as an AND, not a sign test (compare src/b_free_items.cpp).
//
// The index goes in rA of the `stwx` with the array at offset 0, which is the
// FREE-FUNCTION flavour of the operand-order table.

struct Slot
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 val;
};
ASSERT_SIZE(Slot, 8);

struct Table
{
    /* 0x00 */ Slot* slots;
    /* 0x04 */ s32   flags;
    /* 0x08 */ s32   last;
};
ASSERT_OFFSET(Table, flags, 0x04);
ASSERT_OFFSET(Table, last,  0x08);

void ResetTable(Table* t)
{
    s32 n = t->last + 1;

    for (s32 i = 0; i < n; ++i)
        t->slots[i].key = -1;

    t->flags &= (s32)0x80000000;
}
