#include "types.h"

// sub_825F4740 -- initialise a vtable'd object whose fields at +4/+8 are an
// empty circular list anchored on themselves. 48 B, 3 callers.
//
//      lis     r9,-32250
//      li      r11,0
//      addi    r10,r3,4         the list anchor
//      addi    r9,r9,15528      = 82063CA8, this class's vtable
//      stw     r11,12(r3)
//      stw     r10,4(r3)        next = &list
//      stw     r10,8(r3)        prev = &list
//      stw     r9,0(r3)
//      stw     r11,20(r3)
//      stw     r11,24(r3)
//      stw     r11,28(r3)
//      blr
//
// Store order is source order, so the source writes +12 first; the address
// computations are scheduled ahead of the stores that use them. Both list
// fields get the same register because both are the same expression &list.

struct VTable;
extern const VTable kVTable_82063CA8;

struct List
{
    List* next;
    List* prev;
};

struct QueueA
{
    /* 0x00 */ const VTable* vt;
    /* 0x04 */ List           list;
    /* 0x0C */ s32            f12;
    /* 0x10 */ char           unk0010[4];
    /* 0x14 */ s32            f20;
    /* 0x18 */ s32            f24;
    /* 0x1C */ s32            f28;
};

ASSERT_OFFSET(QueueA, list, 4);
ASSERT_OFFSET(QueueA, f12, 12);
ASSERT_OFFSET(QueueA, f20, 20);
ASSERT_OFFSET(QueueA, f28, 28);

void InitQueueA(QueueA* q)
{
    const VTable* v = &kVTable_82063CA8;
    q->f12        = 0;
    q->list.next  = &q->list;
    q->list.prev  = &q->list;
    q->vt         = v;
    q->f20        = 0;
    q->f24        = 0;
    q->f28        = 0;
}
