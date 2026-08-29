#include "types.h"

// sub_8277AA88 -- difference between a field of the element at a stored index
// and the same field of element 0, guarded by three range tests.
// 124 B, 10 callers.
//
//      lwz     r11,20(r3)       i = s->idx
//      mr      r10,r3
//      li      r3,0             the result of every guard is 0
//      cmplwi  cr6,r11,0
//      beqlr   cr6              i == 0 -> 0
//      lwz     r8,4(r10)        n = s->count   (ONE load, used twice)
//      cmplw   cr6,r11,r8       UNSIGNED
//      bge-    cr6,0x8277AAB4
//      cmpwi   cr6,r11,0        SIGNED
//      li      r9,0
//      bge-    cr6,0x8277AAB8
//      li      r9,1
//      clrlwi. r9,r9,24         a materialised bool -> an inlined helper
//      bnelr
//      li      r7,-1
//      subfic  r9,r8,0          CA = (n == 0)
//      subfze  r9,r7            r9 = CA
//      clrlwi. r9,r9,24         a second materialised bool
//      bnelr
//      lwz     r10,0(r10)       items
//      rlwinm  r11,r11,2,0,29
//      lwzx    r11,r11,r10      a = items[i]
//      lwz     r9,0(r11)      ; rlwinm. r9,r9,1,31,31    <-- DEAD
//      lwz     r9,16(r11)       a->stamp
//      lwz     r11,0(r10)       b = items[0]
//      lwz     r10,0(r11)     ; ...
//      lwz     r11,16(r11)      b->stamp
//      rlwinm. r10,r10,1,31,31  <-- DEAD
//      subf    r3,r11,r9        a->stamp - b->stamp
//      blr
//
// `subfic rD,rA,0` + `subfze rD,-1` is a branchless `rA == 0`: subfic leaves
// CA set only when rA is zero, and subfze then copies CA into the register.
// (Compare the idiom table's `addic/subfe`, which is `!= 0`.)  Both bools are
// built as 0/1 and then re-tested with a redundant `clrlwi ...,24`, which is
// what an inlined bool-returning helper leaves behind.
//
// `subf rD,rA,rB` is rB - rA, so the result is items[i]->stamp minus
// items[0]->stamp and not the other way round.
//
// UNEXPLAINED, and it is the whole of the remaining difference (see the
// measurement at the bottom): the two `rlwinm. rX,rY,1,31,31` extract the SIGN
// BIT of field +0 of each element into a register that is overwritten by the
// very next instruction, with CR0 never read. A pure computation whose result
// is dead, twice, in the same shape.

struct SpanNode
{
    /* 0x00 */ u32  flags;
    /* 0x04 */ char unk0004[0x0C];
    /* 0x10 */ s32  stamp;
};

ASSERT_OFFSET(SpanNode, flags, 0x00);
ASSERT_OFFSET(SpanNode, stamp, 0x10);

struct SpanList
{
    /* 0x00 */ SpanNode** items;
    /* 0x04 */ u32        count;
    /* 0x08 */ char       unk0008[0x0C];
    /* 0x14 */ u32        idx;
};

ASSERT_OFFSET(SpanList, items, 0x00);
ASSERT_OFFSET(SpanList, count, 0x04);
ASSERT_OFFSET(SpanList, idx,   0x14);

s32 IndexSpan(SpanList* s)
{
    u32 i = s->idx;
    s32 d = 0;
    if (i != 0)
    {
        u32 n = s->count;
        bool bad = (i >= n || (s32)i < 0);
        if (!bad)
        {
            bool empty = (n == 0);
            if (!empty)
            {
                SpanNode* a = s->items[i];
                SpanNode* b = s->items[0];
                d = a->stamp - b->stamp;
            }
        }
    }

    return d;
}
