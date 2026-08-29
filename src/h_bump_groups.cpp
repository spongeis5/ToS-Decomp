#include "types.h"

// sub_82164040 -- relocate two cursors in each of three 12-byte groups by a
// byte delta, the second cursor only when it is non-null. 120 B, 19 callers.
//
//      lwz     r11,4(r3)   ; add r11,r11,r4 ; stw r11,4(r3)
//      lwz     r10,8(r3)   ; cmplwi cr6,r10,0 ; beq-
//      rotlwi  r11,r10,0   ; add r11,r11,r4 ; stw r11,8(r3)
//      lwz     r10,16(r3)  ; addi r11,r3,12
//      add     r10,r4,r10  ; stw r10,16(r3)
//      lwz     r9,20(r3)   ; cmplwi cr6,r9,0 ; beq-
//      lwz     r10,8(r11)  ; add r10,r10,r4 ; stw r10,8(r11)
//      lwz     r10,28(r3)  ; addi r11,r3,24
//      add     r10,r10,r4  ; stw r10,28(r3)
//      lwz     r9,32(r3)   ; cmplwi cr6,r9,0 ; beqlr
//      lwz     r10,8(r11)  ; add r10,r10,r4 ; stw r10,8(r11)
//      blr
//
// `cmplwi` on the second cursor says pointer, not int.
//
// TWO NESTING LEVELS OF INLINING ARE VISIBLE IN THE ADDRESSING, and getting
// them right is the whole function.
//
// Group 0 reuses the tested value through a `rotlwi r11,r10,0` copy -- the
// common-subexpression fingerprint -- while groups 1 and 2 RELOAD the same
// word they just tested, from a materialised group base (`addi r11,r3,12`,
// then `8(r11)`) instead of the folded `20(r3)` the test used. Nothing stores
// in between, so the reload is not aliasing: it is two address expressions
// that MSVC's CSE sees as different trees, `r3+20` against `(r3+12)+8`.
//
// A flat body -- `s->g[i].head += d; if (s->g[i].tail) s->g[i].tail += d;` --
// folds every base into r3 and comes out 100 B with no reloads, and so does a
// single-level helper taking the group pointer (108 B) or one taking `char**`
// (112 B, but group 0 exact). What keeps `r3+12` alive as a value is a helper
// that takes the GROUP pointer and calls a SECOND inlined helper on a field
// address derived from it. Then the group pointer is a real value with two
// uses inside the guard, so it is materialised, and the tail load hanging off
// it no longer CSEs with the test's `s`-relative load. The head, whose helper
// call is not inside a branch, still folds.
//
// The equivalent member-function spelling (`g[i].Bump(d)` with the same two
// levels) is byte-identical, so `this` is not doing the work here -- the
// nesting is.

struct BumpGroup
{
    /* 0x00 */ s32   unk0000;
    /* 0x04 */ char* head;
    /* 0x08 */ char* tail;
};
ASSERT_OFFSET(BumpGroup, head, 0x04);
ASSERT_OFFSET(BumpGroup, tail, 0x08);
ASSERT_SIZE(BumpGroup, 12);

struct BumpSet
{
    BumpGroup g[3];
};

static void BumpPtr(char** p, s32 d)
{
    *p += d;
}

static void BumpPair(BumpGroup* g, s32 d)
{
    BumpPtr(&g->head, d);
    if (g->tail)
        BumpPtr(&g->tail, d);
}

void BumpAll(BumpSet* s, s32 d)
{
    BumpPair(&s->g[0], d);
    BumpPair(&s->g[1], d);
    BumpPair(&s->g[2], d);
}
