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
//   move to itself, the CSE-copy fingerprint from MATCHED.md -- the 16-bit
//   flavour of `rlwinm rD,rS,0,0,31`.
//
// Both failure exits branch FORWARD to one shared `li r3,0 ; blr`, which per
// the branch-polarity lever means the failure path is written LAST.
//
// NOT MATCHED: 8 of 19 words at /O2, 76 bytes against 68 -- up from 1 of 15
// at 60 bytes, and the two things that moved it are worth keeping.
//
// 1. AN INLINED HELPER TAKING THE NODE POINTER is what produces `mr r3,r11`.
//    Flat, MSVC walks the list in r3 itself (`lwz r3,16(r3)`), returns
//    without a copy, and turns the peeled test into `beqlr` -- 60 bytes, 15
//    words, two short. Passing the head to a static `Walk(TagNode*, s32)`
//    keeps the walk in r11 and copies at the found exit, which is the
//    target's shape. Ten flat spellings were measured and none produces the
//    copy: the tag in a u16 or s32 local, `for(;;)` with the test first and
//    with the test peeled, `<= 22`, an s16 field with casts, the member
//    form, an accumulator, and the head spelled twice.
//
// 2. THE GUARD'S POLARITY places the shared zero return. With
//    `if (p == 0) return 0;` first, MSVC plants `li r3,0 ; blr` immediately
//    after the head test and the whole body shifts; with
//    `if (p != 0) return Walk(p, key); return 0;` the guard branches forward
//    to a zero return at the END, which is the target. That alone is 4 words.
//
// WHAT IS LEFT is two words, and they are the same two at every spelling:
//
//   * the inlined helper's own `return 0` comes out as `li r11,0 ; mr r3,r11`
//     -- a THIRD exit block -- instead of merging with the outer `li r3,0`.
//     The target has exactly two exits, so retail's `< 23` failure and its
//     head-null failure are one block. Six shapes aimed at this were
//     measured (assigning the helper's result to `p` and returning `p`, the
//     guard before the assignment, a failure-value parameter, walking a
//     second pointer, the helper taking the list, and a ternary guard); the
//     assignment forms collapse the helper back to the flat 60-byte code and
//     the rest leave the third block where it is.
//   * the `clrlwi r10,r10,16`. Eleven spellings of the tag reads do not
//     produce it: a u16 local, an s32 local, two u16 locals, an explicit
//     `(s32)` cast on the loop test, `(u16)` casts on both, a u16-parameter
//     `Match(u16, s32)` predicate, a `u16 Tag(TagNode*)` accessor used at
//     both reads, and an unsigned `key`. MSVC folds every one of them into
//     the `lhz`, which already zero-extends.
//
// At /O2 /Os the body is exactly 68 bytes and also 8 of 17, but the loop is
// NOT rotated -- it opens with `b` to a bottom test -- so the eight that
// agree there are a different eight. /O2 has the loop right, which is the
// half worth keeping.
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

static TagNode* Walk(TagNode* p, s32 key)
{
    while (p->tag != key)
    {
        p = p->next;
        if (p->tag < 23)
            return 0;
    }
    return p;
}

TagNode* FindTag(TagList* h, s32 key)
{
    TagNode* p = h->head;

    if (p != 0)
        return Walk(p, key);

    return 0;
}
