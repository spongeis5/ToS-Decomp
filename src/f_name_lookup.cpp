// sub_821559D8 -- linear search of one group's name table for a string,
// returning the 16-bit id stored beside the name, or -1. 140 B, 27 callers.
//
//      rlwinm  r11,r4,1,0,30
//      add     r11,r4,r11
//      rlwinm  r11,r11,2,0,29     group * 12, built as (g + g*2) * 4
//      add     r11,r11,r3
//      lwz     r6,44(r11)         groups[group].count
//      lwz     r4,40(r11)         groups[group].items -- BEFORE the guard
//      cmpwi   cr6,r6,0
//      ble-    cr6,notfound
//      mr      r8,r4              the walking item pointer
//  O:  lwz     r11,0(r8)          items[i].name
//      mr      r10,r5             ... and the needle, reset every pass
//  I:  lbz     r9,0(r11)
//      lbz     r3,0(r10)
//      cmpwi   cr6,r9,0
//      subf    r9,r3,r9           d = *name - *needle
//      beq-    cr6,out            *name == 0: stop
//      addi    r11,r11,1
//      addi    r10,r10,1
//      cmpwi   cr6,r9,0
//      beq+    cr6,I              equal so far: keep going
// out: cmpwi   cr6,r9,0
//      beq-    cr6,found
//      addi    r7,r7,1
//      addi    r8,r8,12
//      cmpw    cr6,r7,r6
//      blt+    cr6,O
// notfound:
//      li      r3,-1
//      blr
// found:
//      rlwinm  r11,r7,1,0,30
//      add     r11,r7,r11
//      rlwinm  r11,r11,2,0,29     i * 12, recomputed from the index
//      add     r10,r11,r4
//      lhz     r3,8(r10)
//      blr
//
// TWO things decide this one, and both were measured against alternatives:
//
// 1. The inner nine instructions are the CRT `strcmp` expanded as an
//    INTRINSIC, not a hand-written loop and not a call. The shape is the
//    same as sub_826973C8 (src/b_strcmp.cpp), so it reads as if that
//    function had been inlined -- but writing it that way and letting MSVC
//    inline it does NOT reproduce the target. The compiler then notices that
//    `name - needle` is invariant inside the inner loop and walks ONE
//    pointer, indexing the other:
//
//        subf  r9,r5,r10          delta, once per outer pass
//        lbzx  r10,r9,r11         *(needle + delta)
//        lbz   r3,0(r11)
//        addi  r11,r11,1          ... and only one increment
//
//    Four hand-written spellings (return/break/for-with-two-breaks/post-
//    increment), with and without __forceinline, all take that shortcut and
//    all land at 4 to 7 of 35 words. `#include <string.h>` and a plain call
//    to `strcmp` keeps BOTH pointers incrementing and gives 35 of 35. The
//    intrinsic expansion is not the same code the inliner produces.
//
// 2. `t->groups[group]` is spelled out at both field reads. Written as
//    `Group* g = &t->groups[group];` first, the two loads move to opposite
//    sides of the `count <= 0` guard and the score drops from 35/35 to 4/35.
//    The target loads count AND items before the guard, which is what the
//    repeated subscript gives.
//
// The found block recomputes i * 12 from the index instead of reusing the
// walking pointer r8: it sits outside the loop, past the strength reduction,
// exactly as in sub_82254A88.
//
// Both strides are 12 -- `addi r8,r8,12` for an item, and the (x + x*2) * 4
// sequence for a group -- so both sizes are asserted from the code.

#include "types.h"
#include <string.h>

#pragma intrinsic(strcmp)

struct Item
{
    /* 0x00 */ const char* name;
    /* 0x04 */ s32         unk0004;
    /* 0x08 */ u16         id;
    /* 0x0A */ u16         unk000A;
};

ASSERT_OFFSET(Item, name, 0x00);
ASSERT_OFFSET(Item, id, 0x08);
ASSERT_SIZE(Item, 12);

struct Group
{
    /* 0x00 */ Item* items;
    /* 0x04 */ s32   count;
    /* 0x08 */ s32   unk0008;
};

ASSERT_OFFSET(Group, items, 0x00);
ASSERT_OFFSET(Group, count, 0x04);
ASSERT_SIZE(Group, 12);

struct NameTable
{
    /* 0x00 */ char  unk0000[0x28];
    /* 0x28 */ Group groups[1];
};

ASSERT_OFFSET(NameTable, groups, 0x28);

int FindNameId(NameTable* t, int group, const char* name)
{
    int   n = t->groups[group].count;
    Item* items = t->groups[group].items;

    for (int i = 0; i < n; ++i)
        if (strcmp(items[i].name, name) == 0)
            return items[i].id;

    return -1;
}
