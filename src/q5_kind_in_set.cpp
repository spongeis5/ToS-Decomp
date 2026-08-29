#include "types.h"

// sub_821F5EE0 -- two inlined bool predicates over the same `kind` field,
// joined by `||`. 88 B, 8 callers.
//
//      lwz     r11,2240(r3)          k = d->kind        (0x8C0)
//      cmpwi   cr6,r11,2 ; beq- yes1
//      cmpwi   cr6,r11,3 ; beq- yes1
//      cmpwi   cr6,r11,4 ; beq- yes1
//      cmpwi   cr6,r11,5
//      li      r10,0
//      bne-    cr6,done1
// yes1:li      r10,1
// done1:clrlwi r10,r10,24            <- a bool, materialised then masked
//      cmplwi  cr6,r10,0
//      bne-    cr6,yes2
//      cmpwi   cr6,r11,1 ; beq- yes2
//      cmpwi   cr6,r11,6
//      li      r11,0
//      bne-    cr6,done2
// yes2:li      r11,1
// done2:clrlwi r3,r11,24
//      blr
//
// MATCHED, 22 of 22 words.
//
// THE ANSWER: COUNT THE MASKED BOOLS. There are TWO `clrlwi ...,24` here,
// and the first guess was that they are two inlined bool helpers joined by
// an `if`. They are not. One belongs to an inlined helper and the OTHER IS
// THE FUNCTION'S OWN BOOL RETURN, so there is only ONE helper and the last
// two comparisons are further terms of the SAME `||` chain:
//
//      return IsMoving(d) || d->kind == 1 || d->kind == 6;
//
// Two helpers joined by `if (IsMoving(d)) return true; return IsHeld(d);` is
// 96 bytes and 13 of 22: the first chain's true exit gets a PRIVATE
// `li r3,1 ; blr` instead of branching into the shared `li r11,1`, and that
// costs two words and displaces seven more.
//
// The tell is where the true exits go. In the target, the first chain's
// `bne-` and the `d->kind == 1` `beq-` BOTH jump to 821F5F2C, which is the
// `li r11,1` the last term also falls into, and there is exactly one
// `clrlwi r3` after it. Every true exit reaching ONE `li 1` is the
// short-circuit expression; a private `li r3,1 ; blr` is a separate
// statement. That is MATCHED.md's sub_8287E440 note read from the outside:
// there it decided an inner predicate, here it decides how many helpers
// there are.
//
// Ruled out on the way, all wrong: `IsMoving(d) || IsHeld(d)` as a single
// expression (108 bytes -- MSVC materialises the second helper's bool, masks
// it, tests it and rebuilds a third 0/1 from the result); the non-short-
// circuit `|` (80 bytes, an `or` of the two bools with the loads CSEd);
// the ternary `IsMoving(d) ? true : IsHeld(d)` and the if/else into one
// variable (both 96 bytes, identical to the two-return form because MSVC
// tail-duplicates `r = true; return r;` back into `return true`); a
// `bool r = IsMoving(d); if (!r) r = IsHeld(d);` accumulator (88 bytes, the
// right SIZE with the wrong shape -- it masks into r3 and uses `bnelr`);
// and the same initialised to true (92 bytes).
//
// The accumulator shape is worth the warning: it is the only other one that
// hits 88 bytes, so size agreement alone would have picked the wrong source.
//
// cmpwi throughout, so `kind` is signed.  The case order 2,3,4,5 then 1,6 is
// source order; `||` does not get reordered.
//
// 0x8C0 is the same `kind` offset i_state_idle.cpp established, but reached
// directly off the argument here rather than through a pointer at +8.
struct KindObj
{
    /* 0x0000 */ char unk0000[0x8C0];
    /* 0x08C0 */ s32  kind;
};
ASSERT_OFFSET(KindObj, kind, 0x8C0);

static bool IsMoving(const KindObj* d)
{
    return d->kind == 2 || d->kind == 3 || d->kind == 4 || d->kind == 5;
}

bool IsActiveKind(KindObj* d)
{
    return IsMoving(d) || d->kind == 1 || d->kind == 6;
}
