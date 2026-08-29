// sub_82631F30 -- find a pointer in an array and clear its slot. 4 callers.
//
// THE INVENTORY ROW IS 152 BYTES AND COVERS TWO BODIES. This one is 72 bytes,
// and 82631F30 + 72 = 82631F78, where a second frameless function begins --
// see src/n17_find_paired.cpp, which cannot be run through match.py because
// 82631F78 is not in the inventory at all. match.py's can_shrink reconciles
// the row against this body and reports the 20 uncompared words.
//
//      lhz    r9,516(r3)      count, a u16 at +0x204
//      li     r11,0
//      cmpwi  cr6,r9,0        SIGNED -- the peeled `i < count` with i = 0
//      ble-   cr6,notfound
//      lwz    r10,512(r3)     arr, +0x200, HOISTED out of the loop
// loop: lwz   r8,0(r10)
//      cmplw  cr6,r8,r4
//      beq-   cr6,found
//      addi   r11,r11,1
//      addi   r10,r10,4
//      cmpw   cr6,r11,r9
//      blt+   cr6,loop
// notfound:
//      li     r11,-1
// found:
//      lwz    r10,512(r3)     arr again -- the induction pointer ate the first
//      rlwinm r9,r11,2,0,29
//      li     r8,0
//      stwx   r8,r9,r10       arr[i] = 0
//      blr
//
// THE -1 IS NOT A GUARD, IT IS STORED THROUGH. Both the empty-table exit and
// the loop-exhausted exit fall into `li r11,-1`, and the function's only
// store follows unconditionally -- so on a miss this writes `arr[-1]`, the
// word immediately before the array. There is no branch between the `li` and
// the `stwx` and no second `blr`, so this is not a misreading of a guard; it
// is what an inlined `Find` returning -1 whose result is subscribed without
// checking compiles to. Written as a flat loop with a `break` instead, MSVC
// produces 96 bytes and 2 of 24 words, so the two-function shape is also what
// the bytes want.
//
// THE INDEX IS MASKED TO 30 BITS, and this is the one claim here that the
// bytes do not show directly. `stwx r8,r9,r10` puts the INDEX in rA; every
// ordinary spelling puts the base there -- plain subscript, a named index, a
// named array pointer, pointer arithmetic, `(char*)arr + i*4`, an inlined
// `ClearAt(void**, int)` helper, an unsigned index, and the whole thing as a
// member function are all 17 of 18 with that one word inverted, at both
// optimisation levels. MSVC matches `base + (index << scale)` as an
// addressing mode and puts the base in rA; a masked index misses the pattern
// and falls back to a generic add. This is MATCHED.md's sub_8215ED28 lever
// exactly, including that the mask is INVISIBLE: keeping 30 low bits is
// absorbed into the `rlwinm ...,2,0,29` that the `* 4` already needed, so the
// scaling word is byte-identical and only the store's operand order moves.
//
// It is also semantically invisible HERE, which is worth stating because the
// index can be -1: `(-1 & 0x3FFFFFFF) * 4` is 0xFFFFFFFC, and so is `-1 * 4`,
// so the masked and unmasked forms address the same word. The mask is
// therefore evidence about MSVC's addressing-mode matcher and NOT evidence
// that anyone wrote `& 0x3FFFFFFF`; some spelling that misses the pattern was
// used, and this is the one that is known to.
//
// The count is compared with `cmpwi`, signed, and the element with `cmplw`,
// unsigned -- the int/pointer split MATCHED.md notes.
//
// Nothing is relocated; 18 words are compared and 20 belong to the next body.

#include "types.h"

struct SlotTable
{
    /* 0x000 */ char   unk0000[0x200];
    /* 0x200 */ void** arr;
    /* 0x204 */ u16    count;
};
ASSERT_OFFSET(SlotTable, arr, 0x200);
ASSERT_OFFSET(SlotTable, count, 0x204);

static int FindSlot(SlotTable* t, void* key)
{
    for (int i = 0; i < t->count; i++)
        if (t->arr[i] == key)
            return i;
    return -1;
}

void ClearSlot(SlotTable* t, void* key)
{
    t->arr[FindSlot(t, key) & 0x3FFFFFFF] = 0;
}
