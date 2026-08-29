#include "types.h"

// sub_82167A18 -- a three-deep pointer chain tested as one boolean.
// 76 B, 7 callers.
//
//   lwz    r10,0(r3)      m = o->next          (loaded ONCE, r10 all the way)
//   cmplwi cr6,r10,0
//   beq-   cr6,false
//   lwz    r11,0(r10)     m->next
//   cmplwi cr6,r11,0
//   li     r11,1
//   bne-   cr6,test
//  false2:
//   li     r11,0
//  test:
//   clrlwi r11,r11,24     <- the redundant mask on a value already 0 or 1
//   cmplwi cr6,r11,0
//   beq-   cr6,false      ... and it is TESTED, not returned
//   lwz    r11,0(r10)     m->next        RELOADED
//   lwz    r10,0(r11)     m->next->next
//   li     r11,1
//   cmplwi cr6,r10,0
//   bne-   cr6,out
//  false:
//   li     r11,0
//  out:
//   clrlwi r3,r11,24
//   blr
//
// Two things fix the shape:
//
// 1. THE FIRST TERM IS AN INLINED bool-RETURNING HELPER. A materialised 0/1
//    followed by a redundant `clrlwi ...,24` before the test is what one
//    leaves behind; a bare `a && b` written out branches out of each term and
//    never builds a value at all (MATCHED.md, "A materialised-then-masked
//    bool is an inlined helper"). Spelled as `m != 0 && m->next != 0 &&
//    m->next->next != 0` the middle li/li/clrlwi trio does not appear.
//
// 2. BOTH FALSE PATHS SHARE ONE `li r11,0`. The first term's `beq-` jumps to
//    the very block the second term falls into, which is the shared-exit
//    shape of a single `&&` expression returning a bool -- not two statements
//    each with their own `return false` (see sub_8219FCD8 / i_state_idle.cpp
//    for the same operator deciding the outer shape).
//
// The reload of `m->next` in the second term, with nothing stored in between,
// is the CSE-defeat tell: the two reads are spelled differently -- once
// inside the helper, once as the head of the longer chain.

struct Leaf
{
    Leaf* next;
};

struct Mid
{
    Leaf* next;
};

struct LeafOwner
{
    Mid* next;
};

static bool IsLinked(const Mid* m)
{
    return m != 0 && m->next != 0;
}

bool HasLinkedLeaf(const LeafOwner* o)
{
    const Mid* m = o->next;

    return IsLinked(m) && m->next->next != 0;
}
