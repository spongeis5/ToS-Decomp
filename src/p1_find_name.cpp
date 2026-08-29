#include "types.h"

// sub_825FAB98 -- linear search of a 28-byte record array for one whose first
// field is a C string equal to the argument; returns the index, or -1.
// 100 B, 9 callers.
//
//      lwz     r7,80(r3)        count      -- read BEFORE the array pointer
//      mr      r11,r3
//      li      r3,0             i = 0, and r3 IS the result
//      cmpwi   cr6,r7,0
//      ble-    cr6,0x825FABF4   count <= 0 -> the single `li r3,-1`
//      lwz     r8,76(r11)       entries    -- loaded after the guard
//  L1: lwz     r10,0(r8)        e->name
//      mr      r11,r4           a = name
//  L2: lbz     r9,0(r11)        c  = *a
//      lbz     r6,0(r10)             *b
//      cmpwi   cr6,r9,0
//      subf    r9,r6,r9         d  = c - *b
//      beq-    cr6,0x825FABDC   c == 0 -> out of the compare
//      addi    r11,r11,1
//      addi    r10,r10,1
//      cmpwi   cr6,r9,0
//      beq+    cr6,L2           while (d == 0)
//      cmpwi   cr6,r9,0
//      beqlr   cr6              equal -> return i
//      addi    r3,r3,1
//      addi    r8,r8,28         28-byte stride
//      cmpw    cr6,r3,r7
//      blt+    cr6,L1
//      li      r3,-1
//      blr
//
// The inner nine instructions are the `strcmp` INTRINSIC, not a hand-written
// loop: both pointers increment (`addi r11,r11,1 ; addi r10,r10,1`) and both
// bytes are zero-extended. Writing the body out by hand -- exactly
// src/b_strcmp.cpp's shape, which is the same routine standalone -- gets the
// loop-invariant-delta transform instead, `subf r9,r4,r10` plus `lbzx`, and a
// separate `b` for the break: 7 of 25 words and 104 bytes against 100. That is
// the lever MATCHED.md records for sub_826973C8's inlined twin, and it applies
// unchanged here.
//
// `subf rD,rA,rB` is rB - rA, so the difference is (*name - *e->name) and the
// argument is the FIRST operand of the comparison.
//
// The `d == 0` test appears TWICE -- once as the loop-back branch and once as
// the `beqlr` -- which is the intrinsic's early exit followed by the caller's
// own `== 0` test.

#include <string.h>

struct NameEntry
{
    /* 0x00 */ const char* name;
    /* 0x04 */ char        unk0004[0x18];
};

ASSERT_OFFSET(NameEntry, name, 0x00);
ASSERT_SIZE(NameEntry, 28);

struct NameTable
{
    /* 0x00 */ char       unk0000[0x4C];
    /* 0x4C */ NameEntry* entries;
    /* 0x50 */ s32        count;
};

ASSERT_OFFSET(NameTable, entries, 0x4C);
ASSERT_OFFSET(NameTable, count,   0x50);

s32 FindName(NameTable* t, const char* name)
{
    s32 n = t->count;

    for (s32 i = 0; i < n; ++i)
    {
        if (strcmp(name, t->entries[i].name) == 0)
            return i;
    }

    return -1;
}
