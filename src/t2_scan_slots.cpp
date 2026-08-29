#include "types.h"

// sub_8215BC20 -- walk a cursor forward through a four-element global table
// looking for a key; on failure park the cursor at -1. 128 B, 7 callers.
//
//      lwz     r11,0(r3) ; mr r10,r3 ; li r7,-1
//      cmpwi   cr6,r11,4 ; bge- -> fail          the PEELED loop test
//      lis     r11,-32102 ; addi r8,r11,-1628    -> 8299F9A4, the table
//   top:
//      lwz     r11,0(r10)                        c->idx, reloaded each pass
//      addi    r6,r8,8                           + the field offset
//      rlwinm  r9,r11,1 ; add r5,r11,r9 ; rlwinm r3,r5,2   idx * 12
//      lwzx    r9,r3,r6 ; cmpw cr6,r9,r4 ; beq- -> out
//      addi    r11,r11,1 ; stw r11,0(r10) ; mr r7,r11
//      rotlwi  r11,r11,0 ; cmpwi cr6,r11,4 ; blt+ top
//   out:
//      cmpwi   cr6,r7,4 ; beq- fail
//      lwz     r11,0(r10) ; cmpwi cr6,r11,4 ; bge- fail
//      li      r3,1 ; blr
//   fail:
//      li      r11,-1 ; li r3,0 ; stw r11,0(r10) ; blr
//
// `lis` + `addi` + a second `addi` is the idiom for a field inside a global
// array element, and the second addi (8) is the field offset. The index build
// is (i + i*2) * 4 = i * 12, so the element is 12 bytes.
//
// A test peeled out in front with a second copy at the bottom is the rotated
// `while`, not a `do/while`. `rotlwi r11,r11,0` is the CSE-copy fingerprint:
// the incremented cursor is read again by the loop condition rather than
// named once.
//
// Both tail guards branch FORWARD to the shared reset, so the reset is
// written last.

struct Entry
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ s32  key;
};
ASSERT_OFFSET(Entry, key, 0x08);
ASSERT_SIZE(Entry, 12);

extern Entry g_entries[4];

struct Cursor
{
    /* 0x00 */ s32 idx;
};
ASSERT_OFFSET(Cursor, idx, 0x00);

int ScanEntries(Cursor* c, int key)
{
    int last = -1;

    while (c->idx < 4)
    {
        if (g_entries[c->idx].key == key)
            break;
        c->idx = c->idx + 1;
        last = c->idx;
    }

    if (last != 4 && c->idx < 4)
        return 1;

    c->idx = -1;
    return 0;
}
