// sub_8215ED28 -- bounds-checked element fetch through an owned array.
// 48 bytes, 36 callers.
// NOT MATCHED: 11 of 12 words. One instruction, and it is the last one.
//
//      lwz     r11,8(r3)           this->array
//      cmplwi  cr6,r11,0
//      beq-    cr6,fail
//      lwz     r10,12(r11)         array->count
//      cmplw   cr6,r4,r10          UNSIGNED compare
//      bge-    cr6,fail
//      lwz     r11,8(r11)          array->items
//      rlwinm  r10,r4,2,0,29       i * 4
//      lwzx    r3,r10,r11          <-- the one word that differs
//      blr
//  fail:
//      li      r3,0
//      blr
//
// `cmplw` is the unsigned compare, so the index is unsigned and one test
// covers both ends of the range. Both guards branch AWAY to the same
// `return 0`, so the fetch is the fall-through and is written first.
//
// ---------------------------------------------------------------------
// WHAT IS LEFT: the target emits `lwzx r3,r10,r11` (scaled index in rA,
// base in rB); this source emits `lwzx r3,r11,r10`, the same address with
// the two operands swapped. Everything else, all eleven other words, is
// identical.
//
// It is NOT a flag: tools/flagsweep.py compiled 72 combinations and all 72
// give 11/12 with the same single difference.
//
// It IS reachable, and the lever is the member-function form -- but only in
// a shape that cannot be used here. Compiled and read out of the objects:
//
//      free  fn, items at offset 0     lwzx r3,r10,r11   <- target order
//      free  fn, items at offset 4/8/12  lwzx r3,r11,r10
//      MEMBER fn, `items[i]`, offset 8  lwzx r3,r10,r11   <- target order
//
// So `this->items[i]` and `a->items[i]` compile to different operand orders
// for the same address, and a zero displacement does the same thing. That
// is the same phenomenon as the member lever recorded for sub_826C0FC8 in
// MATCHED.md, now visible in a second place and in a second form.
//
// The trouble is that INLINING NORMALISES IT. Sixteen shapes were compiled
// looking for a way to keep the member flavour through the inliner:
// member At() defined in-class, defined out of line, __forceinline, const,
// operator[], a template member, the array embedded at offset 8 of the
// pointed-to object so the member sees offset 0, a base class holding
// items/count, a reference member, a reference local, a union member,
// pointer arithmetic in three spellings including the byte offset written
// first, a computed void*** deref, a const local, a const parameter, and
// the ternary form. Every one of them, once the call is inlined into the
// bounds-checked body, emits `lwzx r3,r11,r10`. The standalone member
// functions in the same objects emit `lwzx r3,r10,r11` -- so the lever is
// real and it does not survive inlining.
//
// A source that reproduces this word therefore has to make the items load
// carry a zero displacement or reach it through `this` without an inlined
// call, and no arrangement of these structures does both while keeping
// items at +8 and count at +12.

#include "types.h"

struct Array
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ void** items;
    /* 0x0C */ u32   count;
};

ASSERT_OFFSET(Array, items, 0x08);
ASSERT_OFFSET(Array, count, 0x0C);

struct Holder
{
    /* 0x00 */ char   unk0000[0x08];
    /* 0x08 */ Array* array;
};

ASSERT_OFFSET(Holder, array, 0x08);

void* GetAt(Holder* h, int i)
{
    if (h->array != 0 && (u32)i < h->array->count)
        return h->array->items[i];
    return 0;
}
