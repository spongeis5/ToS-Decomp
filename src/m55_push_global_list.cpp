// sub_82156728 -- push a node onto a global singly-linked list and bump a
// global counter. 36 bytes, 3 callers.
//
//      lis     r10,-32103
//      lis     r9,-32103
//      lwz     r11,1036(r10)       -> 8299040C, the head
//      stw     r11,8(r3)
//      lwz     r11,1032(r9)        -> 82990408, the count
//      addi    r11,r11,1
//      stw     r3,1036(r10)
//      stw     r11,1032(r9)
//      blr
//
// TWO `lis` for two words FOUR BYTES APART, which is the same reading as
// src/m44_store_two_globals.cpp: adjacent is not the same as one object.
// A struct would have formed the base once and used displacements 0 and 4;
// a separate relocated high half per access is what two distinct symbols
// give, and neither half of a relocated pair can be shared between them.
//
// The count's LOAD sits between the two stores, which costs nothing to
// explain -- two distinct globals cannot alias, so MSVC is free to slide it
// into the slot. The three STORES are in source order: the node's link, then
// the head, then the count.
//
// 4 of 9 words are relocated.

#include "types.h"

struct ListNode
{
    /* 0x00 */ u8        unk0000[8];
    /* 0x08 */ ListNode* next;
};

ASSERT_OFFSET(ListNode, next, 8);

extern ListNode* g_listHead;
extern int       g_listCount;

void PushNode(ListNode* n)
{
    n->next = g_listHead;
    g_listHead = n;
    g_listCount = g_listCount + 1;
}
