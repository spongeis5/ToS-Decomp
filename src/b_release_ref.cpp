// sub_82663370 -- reset the vtable, clear a flag on the referenced node and
// drop its reference count, releasing at zero. 60 bytes, 105 CALLERS.
//
//      lis     r10,-32249
//      lwz     r11,12(r3)          this->node
//      addi    r10,r10,-18540      = 8206B794, a vtable
//      cmplwi  cr6,r11,0
//      stw     r10,0(r3)           this->vt = &kVTable_8206B794
//      beqlr   cr6                 no node: done
//      li      r10,0
//      stb     r10,4(r11)          node->flag = 0
//      lwz     r3,12(r3)           node RELOADED
//      lwz     r11,0(r3)
//      addic.  r11,r11,-1
//      stw     r11,0(r3)           --node->count
//      bnelr                       still referenced: done
//      b       0x82662E08          tail call: destroy it
//
// The reload of `this->node` at 82663390 is the load-bearing detail. A local
// copy of the pointer would stay in a register across the whole body; the
// target re-reads the field because a `char`-typed store went through it, and
// a byte store aliases everything. So the source has to name `this->node`
// again after `node->flag = 0` rather than reuse a local -- and the
// subsequent s32 store to `count` does NOT force a second reload, which is
// consistent with the same alias model.
//
// `addic.` is `x - 1` with the record bit, and `bnelr` returns when the new
// count is non-zero, so the tail call is the fall-through: written as the
// interesting path.

#include "types.h"

struct VTable;
extern const VTable kVTable_8206B794;

struct RefNode
{
    /* 0x00 */ s32 count;
    /* 0x04 */ u8  flag;
};

ASSERT_OFFSET(RefNode, count, 0x00);
ASSERT_OFFSET(RefNode, flag,  0x04);

struct Object
{
    /* 0x00 */ const VTable* vt;
    /* 0x04 */ char          unk0004[0x08];
    /* 0x0C */ RefNode*      node;
};

ASSERT_OFFSET(Object, vt,   0x00);
ASSERT_OFFSET(Object, node, 0x0C);

void DestroyNode(RefNode*);

void ReleaseNode(Object* o)
{
    o->vt = &kVTable_8206B794;
    if (o->node != 0)
    {
        RefNode* n;
        o->node->flag = 0;
        n = o->node;
        if (--n->count == 0)
            DestroyNode(n);
    }
}
