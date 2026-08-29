// sub_826225E8 -- walk a circular list, copy each entry's name into a stack
// buffer and return the entry whose name matches. 128 B, 4 callers.
//
//      lwz    r8,8(r3)          n = l->head.next
//      addi   r7,r3,4           &l->head -- the sentinel, NO null check
//      cmplw  cr6,r8,r7
//      beq-   cr6,zero
// outer:
//      lwz    r3,8(r8)          it = n->item, straight into the return reg
//      addi   r10,r1,-128       buf, BELOW r1 with no frame
//      lwz    r11,0(r3)
//      addi   r11,r11,4         s = it->rec->name
//      subf   r10,r11,r10       delta = buf - s, computed ONCE
// copy:lbz    r9,0(r11)         the SOURCE is the induction pointer
//      cmplwi cr6,r9,0
//      stbx   r9,r10,r11        buf[i] = s[i], reached through the delta
//      addi   r11,r11,1
//      bne+   cr6,copy
//      mr     r10,r4
//      addi   r11,r1,-128
// cmp: lbz    r9,0(r11)         BOTH pointers increment
//      lbz    r6,0(r10)
//      cmpwi  cr6,r9,0
//      subf   r9,r6,r9
//      beq-   cr6,done
//      addi   r11,r11,1
//      addi   r10,r10,1
//      cmpwi  cr6,r9,0
//      beq+   cr6,cmp
// done:cmpwi  cr6,r9,0
//      beqlr  cr6               return it -- r3 still holds it
//      lwz    r8,4(r8)          n = n->next
//      cmplw  cr6,r8,r7
//      bne+   cr6,outer
// zero:li     r3,0
//      blr
//
// WHICH POINTER BECOMES THE INDUCTION VARIABLE IS DECLARATION ORDER, and
// that is the whole of what was hard here. The copy gets a loop-invariant
// delta -- one incrementing pointer, one `subf`, and `stbx` -- and the image
// increments the SOURCE. Written as
//
//     char* d = buf;  const char* s = it->rec->name;
//     while ((*d++ = *s++) != 0) ;
//
// MSVC increments the DESTINATION instead: it computes `buf` twice, loads
// with `lbzx` and stores with a plain `stb`, and the function comes out
// 132 bytes at 5 of 32 with everything after the loop displaced by one word.
// Swapping the two declarations -- `s` first, `d` second, nothing else
// changed -- is 32 of 32. That is MATCHED.md's declaration-order lever
// (sub_826C0F50, where it decided an `add`'s operand order) reaching a
// different decision: which of two pointers the strength reducer keeps.
//
// A NEGATIVE RESULT WORTH KEEPING: `strcpy(buf, it->rec->name)` with
// <string.h> is BYTE-IDENTICAL to that hand-written loop, 32 of 32, with or
// without the source named in a local. So MATCHED.md's rule that an inlined
// hand-written body is distinguishable from the intrinsic is a fact about
// `strcmp`, NOT about `strcpy` -- here the delta transform is what BOTH
// produce, and the bytes do not say which was written. The call is the form
// kept because it claims less. What the bytes DO exclude: an indexed
// `buf[i] = s[i]` loop (148 bytes) and a `do/while` on a saved character
// (124 bytes).
//
// THE COMPARE IS THE `strcmp` INTRINSIC. Both pointers increment, which
// MATCHED.md establishes is the intrinsic; a hand-written compare gets the
// same delta transform the copy has. The `subf r9,r6,r9` is `*p - *q` and the
// difference is tested twice -- once at the loop bottom and once after the
// break -- which is the expansion followed by `== 0`, not two source tests.
//
// THE BUFFER IS IN THE RED ZONE. `addi r10,r1,-128` with no `stwu r1` and no
// `mflr` anywhere: a 128-byte local in a function that is a leaf only because
// both string routines inlined.
//
// THE SENTINEL IS A WHOLE NODE, NOT A BARE LINK. `addi r7,r3,4` is a plain
// add with no null test, and the first node is read from `8(r3)` -- which is
// the sentinel's own `next` at (l+4)+4. One layout produces both constants:
// a Node embedded at +0x04 whose `next` is its second word. MATCHED.md's
// sub_8216E778 note is the same reading.
//
// `lwz r3,8(r8)` puts the item in r3 at the TOP of the loop and nothing
// writes r3 again, so the `beqlr` returns the item itself, not a field of it.
//
// /O2 is settled by size: /O2 /Os is 124 bytes.
//
// Nothing is relocated; all 32 words are compared.

#include "types.h"
#include <string.h>

struct NameRec
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ char name[1];
};
ASSERT_OFFSET(NameRec, name, 0x04);

struct NamedItem
{
    /* 0x00 */ const NameRec* rec;
};

struct ListNode
{
    /* 0x00 */ ListNode*  prev;
    /* 0x04 */ ListNode*  next;
    /* 0x08 */ NamedItem* item;
};
ASSERT_OFFSET(ListNode, next, 0x04);
ASSERT_OFFSET(ListNode, item, 0x08);

struct NamedList
{
    /* 0x00 */ char     unk0000[0x04];
    /* 0x04 */ ListNode head;
};
ASSERT_OFFSET(NamedList, head, 0x04);

NamedItem* FindByName(NamedList* l, const char* want)
{
    char buf[128];

    for (ListNode* n = l->head.next; n != &l->head; n = n->next)
    {
        NamedItem* it = n->item;

        strcpy(buf, it->rec->name);

        if (strcmp(buf, want) == 0)
            return it;
    }

    return 0;
}
