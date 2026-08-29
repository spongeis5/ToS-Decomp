#include "types.h"

// sub_8219FCD8 -- two predicates over the same deep object, both of which
// must be false. 108 B, 13 callers.
//
//      lwz     r10,8(r3)
//      lwz     r11,2260(r10)
//      cmpwi   cr6,r11,0
//      bne-    cr6,one          -> r11 = 1
//      lwz     r11,2264(r10)
//      cmpwi   cr6,r11,0
//      li      r11,0
//      beq-    cr6,done
// one: li      r11,1
// done:clrlwi  r11,r11,24       <- a bool, materialised then masked
//      cmplwi  cr6,r11,0
//      bne-    cr6,zero
//      lwz     r11,2240(r10)
//      cmpwi   cr6,r11,2 ; beq- yes
//      cmpwi   cr6,r11,1 ; beq- yes
//      cmpwi   cr6,r11,6
//      li      r11,0
//      bne-    cr6,done2
// yes: li      r11,1
// done2:clrlwi r11,r11,24
//      li      r3,1
//      cmplwi  cr6,r11,0
//      beqlr   cr6
// zero:li      r3,0
//      blr
//
// The offsets are the ones eq1_2260.cpp and eq1_2264.cpp already established
// -- 0x8D4 and 0x8D8 off a pointer at +8 -- with a third field at 0x8C0.
//
// THE TELL IS THE MATERIALISED BOOL. Written as `if (a || b) return 0;`
// MSVC branches straight out of each term and never builds a value. Here each
// `||` chain is turned into 0 or 1 in r11, masked to 8 bits, and only THEN
// tested -- which is what an inlined bool-returning helper does, not what a
// bare `if` does. The redundant clrlwi after two `li`s that already produce
// 0 or 1 is the giveaway.
//
// cmpwi throughout, so all three fields are signed.
//
// The case order 2, 1, 6 is source order; `||` does not get reordered.
//
// THE OUTER JOIN IS ALSO A `||`, and that is worth twelve words. Written as
// two flat guards --
//
//     if (IsPending(d))      return 0;
//     if (IsBlockingKind(d)) return 0;
//     return 1;
//
// -- MSVC does two things at once: it plants a private `li r3,0 ; blr` right
// after the first test instead of sharing the one at the end, and it turns
// the final `return 1` into a BRANCHLESS `cntlzw ; rlwinm`. 12 of 27.
// Joining them with `||` gives both guards the same forward exit and keeps
// the tail branchy (`li r3,1 ; cmplwi ; beqlr`), which is the target: 27 of
// 27. Same lever as sub_8287E440, seen from the other side -- there the `||`
// form was wanted for the INNER predicate, here for the outer one, and in
// both cases the flat-`if` form is what materialises a value the target
// branches on (or the reverse).
struct IdleDeep
{
    /* 0x0000 */ char unk0000[0x8C0];
    /* 0x08C0 */ s32  kind;
    /* 0x08C4 */ char unk08C4[0x10];
    /* 0x08D4 */ s32  a;
    /* 0x08D8 */ s32  b;
};
ASSERT_OFFSET(IdleDeep, kind, 0x8C0);
ASSERT_OFFSET(IdleDeep, a,    0x8D4);
ASSERT_OFFSET(IdleDeep, b,    0x8D8);

struct IdleTop
{
    /* 0x00 */ char      unk0000[8];
    /* 0x08 */ IdleDeep* deep;
};
ASSERT_OFFSET(IdleTop, deep, 0x08);

static bool IsPending(const IdleDeep* d)
{
    return d->a != 0 || d->b != 0;
}

static bool IsBlockingKind(const IdleDeep* d)
{
    return d->kind == 2 || d->kind == 1 || d->kind == 6;
}

int IsIdle(IdleTop* t)
{
    IdleDeep* d = t->deep;

    if (IsPending(d) || IsBlockingKind(d))
        return 0;
    return 1;
}
