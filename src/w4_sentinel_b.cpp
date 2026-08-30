#include "types.h"

// sub_8258A730 -- the same empty-circular-list initialiser as
// src/w4_sentinel_a.cpp, with a different vtable and three zero fields.
// 44 B, 4 callers.
//
//      lis     r9,-32250
//      li      r10,0
//      addi    r11,r3,4         the list anchor
//      addi    r8,r9,3508       = 82060DB4, this class's vtable
//      stw     r10,12(r3)
//      stw     r11,4(r3)        next = &list
//      stw     r11,8(r3)        prev = &list
//      stw     r8,0(r3)
//      stw     r10,16(r3)
//      stw     r10,20(r3)
//      blr

struct VTable;
extern const VTable kVTable_82060DB4;

struct List
{
    List* next;
    List* prev;
};

struct QueueB
{
    /* 0x00 */ const VTable* vt;
    /* 0x04 */ List           list;
    /* 0x0C */ s32            f12;
    /* 0x10 */ s32            f16;
    /* 0x14 */ s32            f20;
};

ASSERT_OFFSET(QueueB, list, 4);
ASSERT_OFFSET(QueueB, f12, 12);
ASSERT_OFFSET(QueueB, f20, 20);

void InitQueueB(QueueB* q)
{
    q->f12       = 0;
    q->list.next = &q->list;
    q->list.prev = &q->list;
    q->vt        = &kVTable_82060DB4;
    q->f16       = 0;
    q->f20       = 0;
}
