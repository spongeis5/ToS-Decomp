#include "types.h"

// sub_825FA9E8 -- drain a pending queue backwards into a compacted array,
// de-duplicating by key through a lookup table. 176 B, 8 callers.
// Takes no arguments: r3 is overwritten with a `lis` before it is ever read.
//
//      lis     r31,-32092
//      lwz     r7,-23188(r31)  n = g_pendingCount          (82A3A56C)
//      cmpwi   cr6,r7,0
//      ble-    cr6,ret         nothing pending -- and the two write-backs
//                              at the end are SKIPPED, so this is a real
//                              early return, not a rotated loop guard
//      lis     r3,-32092
//      lis     r10,-32092
//      lis     r11,-32092
//      addi    r9,r10,-23184   g_pending                   (82A3A570)
//      addi    r4,r11,-8600    g_items                     (82A3DE68)
//      rlwinm  r10,r7,3,0,28   n * 8
//      lwz     r11,-10224(r3)  m = g_itemCount             (82A3D810)
//      add     r10,r10,r9      &g_pending[n]
//      rlwinm  r9,r11,3,0,28   m * 8
//      add     r9,r9,r4
//      addi    r6,r9,-8        &g_items[m] - 8, biased for stdu
//      lis     r9,-32092
//      addi    r5,r9,-5368     g_slots                     (82A3EB08)
//  L:  lhzu    r9,-8(r10)      --n ; key = g_pending[n].key
//      addi    r7,r7,-1
//      cmplwi  cr6,r9,65535    UNSIGNED against 0xFFFF
//      beq-    cr6,done        break
//      rlwinm  r9,r9,3,0,28    key * 8
//      lhzx    r8,r9,r5
//      extsh   r8,r8
//      cmpwi   cr6,r8,-1       SIGNED against -1
//      bne-    cr6,seen
//      ld      r8,0(r10)       the whole 8-byte entry
//      mr      r30,r11         k = m
//      addi    r11,r11,1       ++m
//      sthx    r30,r9,r5       g_slots[key].idx = (s16)k
//      stdu    r8,8(r6)        g_items[k] = g_pending[n]
//      b       bottom
// seen:ld      r9,0(r10)
//      rlwinm  r8,r8,3,0,28
//      stdx    r9,r8,r4        g_items[slot] = g_pending[n]
// bot: cmpwi   cr6,r7,0
//      bgt+    cr6,L
// done:stw     r7,-23188(r31)
//      stw     r11,-10224(r3)
// ret:
//
// Three things are readable and each costs a word if guessed:
//
// * THE SIGNEDNESS SPLIT. The queue key is compared `cmplwi` against 65535
//   and the table entry `extsh`/`cmpwi` against -1, so the two 16-bit fields
//   have DIFFERENT types -- u16 in the queue element, s16 in the table.
//
// * The element is moved with one `ld`/`std` pair, so it is an 8-byte struct
//   assigned whole, not two word stores (compare the vector-copy note in
//   MATCHED.md: the copy instruction says the width).
//
// * `g_items[m]` IS STRENGTH-REDUCED to the `stdu` induction pointer r6,
//   set up outside the loop by `rlwinm`/`add`/`addi -8`, while `g_items[slot]`
//   on the other arm stays indexed off the base in r4.  Naming the index in a
//   local first -- `s32 k = m++; g_items[k] = ...` -- takes that away: k is
//   not an induction variable, so both arms index and the whole function is
//   168 bytes against 176, 2 of 42 words.  `++m` written after the two stores
//   is what leaves m as the induction variable; MSVC then hoists the add and
//   keeps the pre-increment value in r30 for the `sthx` (r30 rather than a
//   volatile because r4-r11 are all live across the loop).

struct Pending
{
    /* 0x00 */ u16 key;
    /* 0x02 */ u16 f02;
    /* 0x04 */ u32 f04;
};
ASSERT_SIZE(Pending, 8);

struct Slot
{
    /* 0x00 */ s16 idx;
    /* 0x02 */ u16 f02;
    /* 0x04 */ u32 f04;
};
ASSERT_SIZE(Slot, 8);

extern s32     g_pendingCount;
extern Pending g_pending[];
extern s32     g_itemCount;
extern Pending g_items[];
extern Slot    g_slots[];

void FlushPending(void)
{
    s32 n = g_pendingCount;
    if (n <= 0)
        return;

    s32 m = g_itemCount;

    while (n > 0)
    {
        --n;

        u16 key = g_pending[n].key;
        if (key == 0xFFFF)
            break;

        s16 slot = g_slots[key].idx;
        if (slot == -1)
        {
            g_slots[key].idx = (s16)m;
            g_items[m] = g_pending[n];
            ++m;
        }
        else
        {
            g_items[slot] = g_pending[n];
        }
    }

    g_pendingCount = n;
    g_itemCount = m;
}
