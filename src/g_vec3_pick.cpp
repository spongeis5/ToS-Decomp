#include "types.h"

// sub_8220F810 -- copy three consecutive words to an out parameter, taken from
// a linked object when there is one and from the object itself otherwise.
// 72 B, 14 callers.
//
//      lwz     r11,156(r3)      o->link  (+0x9C)
//      cmpwi   cr6,r11,0
//      beq-    cr6,0x8220F83C
//      lwz     r11,56(r11)      link->node (+0x38)
//      lwz     r10,48(r11)      +0x30
//      stw     r10,0(r4)
//      lwz     r9,52(r11)
//      stw     r9,4(r4)
//      lwz     r8,56(r11)
//      stw     r8,8(r4)
//      blr
//  0x8220F83C:
//      lwz     r11,68(r3)       o's own triple (+0x44)
//      stw     r11,0(r4)
//      lwz     r10,72(r3)
//      stw     r10,4(r4)
//      lwz     r9,76(r3)
//      stw     r9,8(r4)
//      blr
//
// lwz/stw and not lfs/stfs, in load-store pairs: three separate word field
// assignments, the shape of src/copy3_68.cpp.
//
// The `beq-` jumps AWAY to the fallback, so the linked path is the
// fall-through and is written FIRST.
//
// The last word was `cmpwi` against our `cmplwi`: a plain `if (o->link)` on a
// pointer is an UNSIGNED compare, and the target's is signed. Casting the
// pointer -- `if ((s32)o->link)` -- is the whole of it, 17/18 to 18/18, and
// the pointer is still dereferenced out of the same register afterwards.

struct Triple { s32 x; s32 y; s32 z; };

struct PickNode
{
    /* 0x00 */ char unk0000[0x30];
    /* 0x30 */ s32  x;
    /* 0x34 */ s32  y;
    /* 0x38 */ s32  z;
};

struct PickLink
{
    /* 0x00 */ char      unk0000[0x38];
    /* 0x38 */ PickNode* node;
};

struct PickOwner
{
    /* 0x00 */ char       unk0000[0x44];
    /* 0x44 */ s32        x;
    /* 0x48 */ s32        y;
    /* 0x4C */ s32        z;
    /* 0x50 */ char       unk0050[0x4C];
    /* 0x9C */ PickLink*  link;
};

ASSERT_OFFSET(PickNode,  x,    0x30);
ASSERT_OFFSET(PickLink,  node, 0x38);
ASSERT_OFFSET(PickOwner, x,    0x44);
ASSERT_OFFSET(PickOwner, link, 0x9C);

void GetTriple(PickOwner* o, Triple* d)
{
    if ((s32)o->link)
    {
        PickNode* s = o->link->node;
        d->x = s->x;
        d->y = s->y;
        d->z = s->z;
    }
    else
    {
        d->x = o->x;
        d->y = o->y;
        d->z = o->z;
    }
}
