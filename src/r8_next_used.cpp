#include "types.h"

// sub_82724620 -- scan forward from i+1 for the first slot that is not -1,
// returning the index it stopped at. 64 B, 7 callers.
//
// Neighbour of src/ring_index.cpp (827245C0) and src/ring_index2.cpp
// (827245E0), both /O2, and the same layout: items at +0, a bound at +8.
//
//      lwz     r9,8(r3)        n = r->last          read ONCE
//      mr      r11,r3          r3 is about to become the result
//      addi    r3,r4,1         k = i + 1
//      cmpw    cr6,r3,r9
//      bgtlr   cr6             past the end: return k
//      lwz     r10,0(r11)      r->items, HOISTED -- nothing stores in the loop
//      rlwinm  r11,r3,2,0,29
//      add     r11,r11,r10     &items[k]
//  L:  lwz     r10,0(r11)
//      cmpwi   cr6,r10,-1      SIGNED
//      bnelr   cr6             found: return k
//      addi    r3,r3,1
//      addi    r11,r11,4
//      cmpw    cr6,r3,r9
//      ble+    cr6,L
//      blr                     ran off the end: return n + 1
//
// Guard-then-do/while with the bound in a register the whole way is the
// rotated `while`, and every exit returns the same variable, which is why the
// compiler built it in r3 before it was finished with the object pointer.
//
// The loop body's early exit is a `bnelr`, a guard written as a conditional
// RETURN with the interesting path falling through, so `break` (not an
// inverted test) is what the source spells.

struct Ring
{
    /* 0x00 */ s32* items;
    /* 0x04 */ char unk0004[0x04];
    /* 0x08 */ s32  last;
};
ASSERT_OFFSET(Ring, items, 0x00);
ASSERT_OFFSET(Ring, last,  0x08);

s32 NextUsed(Ring* r, s32 i)
{
    s32 n = r->last;
    s32 k = i + 1;

    while (k <= n)
    {
        if (r->items[k] != -1)
            break;
        ++k;
    }

    return k;
}
