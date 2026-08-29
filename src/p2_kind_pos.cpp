#include "types.h"

// sub_822165C0 -- fetch a three-word triple that depends on a kind code held
// by the object hanging off the argument. 128 B, 10 callers.
//
//      lwz     r9,0(r3)         o = h->obj
//      mr      r11,r4           out
//      lwz     r10,32(r9)       k = o->kind
//      cmplwi  cr6,r10,85
//      bne-    cr6,0x822165E4
//        lwz   r4,56(r9)        o->node
//        li    r5,0
//        mr    r3,r11
//        b     0x8217E950       tail call Something(out, o->node, 0)
//      cmplwi  cr6,r10,56
//      beq-    cr6,0x82216620
//      cmplwi  cr6,r10,90
//      beq-    cr6,0x82216620
//      cmplwi  cr6,r10,86
//      beq-    cr6,0x82216620
//      lis     r10,-32256
//      addi    r9,r10,11376     &g_default  (0x82002C70)
//      lwz     r8,11376(r10)  ; stw r8,0(r11)
//      lwz     r7,4(r9)       ; stw r7,4(r11)
//      lwz     r6,8(r9)       ; stw r6,8(r11)
//      blr
//  0x82216620:
//      lwz     r10,56(r9)       o->node
//      lwz     r9,48(r10)     ; stw r9,0(r11)
//      lwz     r8,52(r10)     ; stw r8,4(r11)
//      lwz     r7,56(r10)     ; stw r7,8(r11)
//      blr
//
// Read off the listing:
//   * every compare is `cmplwi`, so the kind is UNSIGNED.
//   * lwz/stw pairs, not lfs/stfs, so three word fields (compare
//     src/g_vec3_pick.cpp, the same copy shape).
//   * `lis` + `addi` forming an address, with the first load folded into the
//     `lis`'s own displacement, is the ADDRESS OF A GLOBAL OBJECT, not a read
//     of a global pointer (compare src/global_field.cpp against
//     src/b_fwd_global5.cpp).
//   * the three `beq-` jump FORWARD past the fallback block, so the fallback
//     is the fall-through and the `||` chain's body is written second -- an
//     if/else, with the disjunction first.
//
// It is not a switch: MSVC lays switch case bodies out in source order, which
// would have put the 56/90/86 body before the default.
//
// THE LEVER, and it is a new one: the fallback block is a WHOLE-STRUCT
// ASSIGNMENT and the node block is three scalar assignments, and the two are
// distinguishable. Written as three scalar reads of the global's members, the
// fallback loads all three words into r10 -- recycling the register the `lis`
// left dead -- where the target uses a fresh r8/r7/r6. That is 5 wrong words
// of 32 and no flag moves it (/Os reuses registers even harder). `*out =
// g_defaultTriple;` emits the identical three lwz/stw pairs and allocates the
// fresh descending registers: 32 of 32. The node block above matched as three
// scalar assignments in the same compile, so this is not a global property of
// the function -- the two blocks really were written differently.

struct Triple { s32 x; s32 y; s32 z; };

struct KindNode
{
    /* 0x00 */ char unk0000[0x30];
    /* 0x30 */ s32  x;
    /* 0x34 */ s32  y;
    /* 0x38 */ s32  z;
};

ASSERT_OFFSET(KindNode, x, 0x30);

struct KindObj
{
    /* 0x00 */ char      unk0000[0x20];
    /* 0x20 */ u32       kind;
    /* 0x24 */ char      unk0024[0x14];
    /* 0x38 */ KindNode* node;
};

ASSERT_OFFSET(KindObj, kind, 0x20);
ASSERT_OFFSET(KindObj, node, 0x38);

struct KindHolder
{
    /* 0x00 */ KindObj* obj;
};

ASSERT_OFFSET(KindHolder, obj, 0x00);

extern Triple g_defaultTriple;

void ProjectPoint(Triple* out, KindNode* n, s32 mode);

void GetKindTriple(KindHolder* h, Triple* out)
{
    KindObj* o = h->obj;
    u32 k = o->kind;

    if (k == 85)
    {
        ProjectPoint(out, o->node, 0);
        return;
    }

    if (k == 56 || k == 90 || k == 86)
    {
        KindNode* n = o->node;
        out->x = n->x;
        out->y = n->y;
        out->z = n->z;
    }
    else
    {
        *out = g_defaultTriple;
    }
}
