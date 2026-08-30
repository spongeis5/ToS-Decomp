#include "types.h"

// sub_82806CB0 -- 88 B, 132 callers. A BRIDGE between sub_82806C98
// (src/vt_acc_12.cpp, Acc_82806C98) and sub_82806D08
// (src/a_report_badthis.cpp).
//
//      mflr r12 ; stw r12,-8(r1) ; std r31,-16(r1) ; stwu r1,-96(r1)
//      lwz     r11,8(r3)           h->child
//      mr      r31,r4              id, live across the call
//      cmplwi  cr6,r11,0
//      beq-    cr6,zero
//      rotlwi  r3,r11,0            <- rlwinm rD,rS,0,0,31: a CSE COPY
//      lwz     r11,0(r3)           child->vt
//      lwz     r11,8(r11)          slot[2]
//      mtctr   r11 ; bctrl
//      cmpw    cr6,r3,r31          SIGNED: both sides are ints
//      li      r3,1
//      beq-    cr6,out
// zero:li      r3,0
// out: epilogue
//
// Three readings decide the shape:
//
// * `rotlwi r3,r11,0` is MATCHED.md's fingerprint of a REPEATED
//   sub-expression: naming `h->child` in a local lets MSVC load straight
//   into r3 and the copy disappears. So `h->child` is spelled out at each
//   of its three uses.
// * Both exits share ONE `li r3,0` and the true path a single `li r3,1`,
//   with no private `li r3,x ; blr` anywhere -- the short-circuit form, not
//   a sequence of statements.
// * There is NO trailing `clrlwi r3,r3,24`, and the 0/1 is computed
//   straight into r3. That says the return type is NOT `bool`: a bool
//   return normalises through a scratch register. `int` is what fits.
//
// `cmpw` rather than `cmplw` makes the vtable call's result and the second
// argument signed ints, so this is an id comparison and not a pointer one.
//
// NEEDS /O2 /Os, and by the loudest of the recorded signatures: at plain
// /O2 the source is 20 of 22 words with every instruction and every branch
// already right, and the two that differ are one value's register --
// `lwz r10,8(r11)` / `mtctr r10` where the target reuses r11 for the slot
// it just loaded the vtable into. Fresh-where-retail-reuses is the flag,
// not the source; no line of C was touched between the two runs.

struct IdNode;

typedef int (*IdFn)(IdNode*);

struct IdVT
{
    IdFn slot[4];
};

struct IdNode
{
    const IdVT* vt;
};
ASSERT_OFFSET(IdNode, vt, 0x00);

struct IdHolder
{
    /* 0x00 */ char    unk0000[0x08];
    /* 0x08 */ IdNode* child;
};
ASSERT_OFFSET(IdHolder, child, 0x08);

int ChildIdIs(IdHolder* h, int id)
{
    return h->child != 0 && h->child->vt->slot[2](h->child) == id;
}
