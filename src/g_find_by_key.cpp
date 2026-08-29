#include "types.h"

// sub_8260FEB0 -- scan a node list for the entry whose key matches, unless a
// flag bit says the container is disabled. 76 B, 16 callers.
//
//      lbz     r11,1040(r3)     c->flags   (+0x410)
//      mr      r10,r3
//      li      r3,0             found = 0, before the guard
//      rlwinm  r9,r11,0,25,25   & 0x40
//      cmplwi  cr6,r9,0
//      bnelr   cr6              return found
//      lwz     r11,68(r10)      n   = c->head  (+0x44)
//      lwz     r9,124(r10)      end = c->tail  (+0x7C), hoisted, loaded once
//  L:  cmplw   cr6,r11,r9
//      beqlr   cr6
//      lwz     r10,8(r11)       it = n->item
//      lwz     r8,4(r10)        it->key
//      cmplw   cr6,r8,r4
//      bne-    cr6,0x8260FEEC
//      mr      r3,r10           found = it
//      lwz     r11,4(r11)       n = n->next -- runs on BOTH paths
//      cmplwi  cr6,r3,0
//      beq+    cr6,L
//      blr
//
// The advance happens before the found test, so the exit is a `break` AFTER
// `n = n->next`, not a break at the assignment.
//
// Two things had to be right, and neither is visible in what the function
// computes.
//
// 1. THE LOOP IS A do/while, and the back edge is the FOUND test, not the
//    bound test. `while (n != end) { ...; if (found) break; }` and the
//    identical `for (;;)` both get rotated: MSVC peels the bound test to the
//    top, makes the found test a conditional return, and emits a SECOND copy
//    of the bound test at the bottom -- 88 bytes to the target's 76. Writing
//    it as `do { if (n == end) break; ...; } while (found == 0);` puts the
//    bound test inside the body where the target has it and leaves the found
//    test as the back edge. A do/while is the one loop MSVC never rotates.
//
// 2. THE FLAG GUARD IS NOT AN EARLY RETURN. `if (c->flags & 0x40) return
//    found;` gives `beq-` around a local `blr` -- two instructions where the
//    target has one -- because a second return point gets its own epilogue.
//    Inverting it to `if ((c->flags & 0x40) == 0) { ... }` with a single
//    `return found` at the end leaves one epilogue for the guard and both
//    loop exits to share, and the forward branch to a bare `blr` folds into
//    `bnelr`. That is the same rule as branch polarity, one level up: the
//    number of RETURN STATEMENTS decides whether a guard can be a `bclr`.

struct KeyItem
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ u32  key;
};

struct KeyNode
{
    /* 0x00 */ char      unk0000[0x04];
    /* 0x04 */ KeyNode*  next;
    /* 0x08 */ KeyItem*  item;
};

struct KeyList
{
    /* 0x000 */ char     unk0000[0x44];
    /* 0x044 */ KeyNode* head;
    /* 0x048 */ char     unk0048[0x34];
    /* 0x07C */ KeyNode* tail;
    /* 0x080 */ char     unk0080[0x390];
    /* 0x410 */ u8       flags;
};

ASSERT_OFFSET(KeyItem, key,   0x04);
ASSERT_OFFSET(KeyNode, next,  0x04);
ASSERT_OFFSET(KeyNode, item,  0x08);
ASSERT_OFFSET(KeyList, head,  0x44);
ASSERT_OFFSET(KeyList, tail,  0x7C);
ASSERT_OFFSET(KeyList, flags, 0x410);

KeyItem* FindByKey(KeyList* c, u32 key)
{
    KeyItem* found = 0;

    if ((c->flags & 0x40) == 0)
    {
        KeyNode* n   = c->head;
        KeyNode* end = c->tail;

        do
        {
            if (n == end)
                break;
            KeyItem* it = n->item;
            if (it->key == key)
                found = it;
            n = n->next;
        } while (found == 0);
    }
    return found;
}
