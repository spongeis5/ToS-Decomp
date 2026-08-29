#include "types.h"

// sub_826CEA98 -- walk a chain of tagged nodes for one whose 16-bit tag
// equals the key, stopping when a tag falls below 23. 68 B of code; the
// recorded 128 covers a second body at 826CEAE0 after a zero pad.
// 8 callers.
//
//      lwz     r11,16(r3)      p = h->head
//      cmplwi  cr6,r11,0
//      beq-    cr6,null
//      lhz     r10,0(r11)
//      cmpw    cr6,r10,r4      SIGNED: key is an int
//      beq-    cr6,found
//  L:  lwz     r11,20(r11)     p = p->next
//      lhz     r10,0(r11)
//      cmplwi  cr6,r10,23
//      blt-    cr6,null
//      clrlwi  r10,r10,16      <- a move to ITSELF
//      cmpw    cr6,r10,r4
//      bne+    cr6,L
// found:mr     r3,r11
//      blr
// null: li     r3,0
//      blr
//
// Two things are readable here and both decide the source shape:
//
// * The peeled `p->tag == key` test in front of the loop, with the back edge
//   testing the same thing, is a ROTATED `while (p->tag != key)`. The head
//   node therefore gets no `< 23` range check -- that check is the first
//   statement of the body, after the `next` step.
// * `clrlwi r10,r10,16` with the same register on both sides is a register
//   move to itself, the CSE-copy fingerprint from MATCHED.md. The loop body
//   reads `p->tag` TWICE (once for the range test, once for the equality),
//   spelled out both times; naming it in a local removes the copy.
//
// Both failure exits branch FORWARD to one shared `li r3,0 ; blr`, which per
// the branch-polarity lever means the failure path is written LAST.
struct TagNode
{
    /* 0x00 */ u16      tag;
    /* 0x02 */ char     unk0002[0x12];
    /* 0x14 */ TagNode* next;
};
ASSERT_OFFSET(TagNode, next, 0x14);

struct TagList
{
    /* 0x00 */ char     unk0000[0x10];
    /* 0x10 */ TagNode* head;
};
ASSERT_OFFSET(TagList, head, 0x10);

TagNode* FindTag(TagList* h, s32 key)
{
    TagNode* p = h->head;

    if (p != 0)
    {
        while (p->tag != key)
        {
            p = p->next;
            if (p->tag < 23)
                return 0;
        }
        return p;
    }

    return 0;
}
