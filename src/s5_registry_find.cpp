#include "types.h"

// sub_821AAFA8 -- scan a fixed slot array in a global object for the entry
// that is NOT the one passed in but carries the same id. 92 B, 7 callers.
//
//   lis   r11,-32108
//   li    r9,0                i = 0
//   lwz   r7,-24180(r11)      g = g_registry        (a global POINTER)
//   lwz   r8,1080(r7)         n = g->count   (+0x438, SIGNED: cmpwi/ble-)
//   cmpwi cr6,r8,0
//   ble-  cr6,none
//   addi  r11,r7,1064         p = &g->slots[0]      (+0x428)
// L:lwz   r10,0(r11)          v = *p
//   cmplw cr6,r10,r3          UNSIGNED -- a pointer compare
//   beq-  cr6,next            v == self: skip
//   lwz   r10,764(r10)        v->id          (+0x2FC)
//   cmpw  cr6,r10,r4          SIGNED -- an int compare
//   beq-  cr6,hit
// next:
//   addi  r9,r9,1
//   addi  r11,r11,4
//   cmpw  cr6,r9,r8
//   blt+  cr6,L
// none:
//   li    r3,0
//   blr
// hit:
//   addi   r11,r9,266         i + 1064/4
//   rlwinm r10,r11,2,0,29     * 4
//   lwzx   r3,r10,r7          g->slots[i], REBUILT from the index
//   blr
//
// The array offset folds INTO the index (266 = 1064/4) because the elements
// are 4 bytes and the offset is a multiple of 4, so the base register is the
// global pointer itself. In the loop it is strength-reduced to `addi
// r11,r7,1064` plus `addi r11,r11,4`; at the hit -- an out-of-line block --
// the address is rebuilt from `i` instead, which is `g->slots[i]` written out
// a third time rather than the walking pointer being reused.
//
// The two compares have DIFFERENT signedness on purpose: `cmplw` against the
// argument pointer, `cmpw` against the id. That is two field types, not one.
//
// The global is read with `lis` + `lwz` and the low half stays in the load's
// displacement, so it is a pointer VARIABLE being dereferenced, not the
// address of a global object (which would be `lis` + `addi`).
//
// Slot count: the count sits 16 bytes after the array, so the array holds
// exactly four pointers. Both offsets are asserted; nothing else about the
// object is known.
//
// THE ONE EDIT THAT DECIDED IT: the loop body NAMES the slot in a local and
// the return statement spells the subscript out again. With
// `g_registry->slots[i]` written at all three uses the two inside the body
// become a common subexpression and MSVC copies it -- `rotlwi r7,r7,0`, a
// move to itself -- which costs a register, so the id load lands in a FRESH
// one (`lwz r6,764(r7)`) instead of overwriting its own input. That is one
// extra instruction, 96 bytes against 92, and every register downstream
// renamed. Naming it makes the slot dead after `e->id` and the load reuses
// its register, which is the target's `lwz r10,764(r10)`.
//
// The RETURN must stay written out: with `return e;` the slot would still be
// live at the hit and the block would be a single `mr r3,rE`, where the
// target rebuilds the address from the index in three instructions.
//
// Neither level fixes this -- /O2 /Os keeps the copy and merely folds the id
// load's destination. It is the naming, not the flag.

struct RegEntry
{
    /* 0x000 */ char unk0000[0x2FC];
    /* 0x2FC */ s32  id;
};
ASSERT_OFFSET(RegEntry, id, 0x2FC);

struct Registry
{
    /* 0x000 */ char      unk0000[0x428];
    /* 0x428 */ RegEntry* slots[4];
    /* 0x438 */ s32       count;
};
ASSERT_OFFSET(Registry, slots, 0x428);
ASSERT_OFFSET(Registry, count, 0x438);

extern Registry* g_registry;

RegEntry* FindOtherWithId(RegEntry* self, s32 id)
{
    for (s32 i = 0; i < g_registry->count; ++i)
    {
        RegEntry* e = g_registry->slots[i];
        if (e != self && e->id == id)
            return g_registry->slots[i];
    }

    return 0;
}
