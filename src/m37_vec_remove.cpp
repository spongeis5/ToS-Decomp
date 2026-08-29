// sub_8268D810 -- find a pointer in a vector and erase it by shifting the
// tail down. 120 bytes, 4 callers.
//
//      lwz  r9,152(r3)          the count, read straight off r3
//      addi r8,r3,148           and then EVERYTHING through r8
//      li   r11,0
//      cmpwi cr6,r9,0 ; ble- cr6,<notfound>
//      lwz  r10,0(r8)
//  L:  lwz r7,0(r10) ; cmplw cr6,r7,r4 ; beq- cr6,<erase>
//      addi r11,r11,1 ; addi r10,r10,4 ; cmpw cr6,r11,r9 ; blt+ cr6,L
//  notfound:
//      li   r11,-1
//  erase:
//      lwz r10,4(r8) ; addi r10,r10,-1 ; stw r10,4(r8)
//      cmpw cr6,r11,r10 ; bgelr cr6
//      rlwinm r10,r11,2,0,29
//  L2: lwz r9,0(r8) ; addi r11,r11,1 ; add r9,r9,r10 ; addi r10,r10,4
//      lwz r7,4(r9) ; stw r7,0(r9)
//      lwz r6,4(r8) ; cmpw cr6,r11,r6 ; blt+ cr6,L2
//      blr
//
// `addi r8,r3,148` with every later access at 0(r8) and 4(r8) is a sub-struct
// address handed to an inlined helper, not two fields of the outer object --
// the two-level shape from sub_82164040. The very first count read still goes
// through r3 at +152 because the `addi` had not issued yet; that is
// scheduling, and it is the reason 148 and 152 both appear.
//
// The -1 really is fed to the erase: the search's not-found value is not
// re-checked, so a miss shifts from index -1. That is what the image does and
// the source has to say it -- two inlined helpers, one returning -1, and a
// caller that passes the result straight in.
//
// The search loop hoists both the base and the count, because nothing stores.
// The shift loop reloads both every iteration, because its store can alias
// them. Same function, both behaviours, and neither needs a lever.
//
// `cmplw` for the pointer compare, `cmpw` for the index tests: signed index,
// unsigned pointers.
//
// Nothing is relocated: 30 of 30 words are compared.

#include "types.h"

struct PtrVec
{
    /* 0x00 */ void** items;
    /* 0x04 */ int    count;
};

ASSERT_OFFSET(PtrVec, count, 0x04);

struct VecOwner
{
    /* 0x00 */ u8     unk0000[148];
    /* 0x94 */ PtrVec list;
};

ASSERT_OFFSET(VecOwner, list, 148);

static int VecIndexOf(PtrVec* v, void* key)
{
    for (int i = 0; i < v->count; i++)
    {
        if (v->items[i] == key)
            return i;
    }

    return -1;
}

static void VecEraseAt(PtrVec* v, int i)
{
    v->count = v->count - 1;

    for (; i < v->count; i++)
        v->items[i] = v->items[i + 1];
}

void VecOwnerRemove(VecOwner* o, void* key)
{
    VecEraseAt(&o->list, VecIndexOf(&o->list, key));
}
