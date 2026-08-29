// sub_8215ED28 -- bounds-checked element fetch through an owned array.
// 48 bytes, 36 callers.
// MATCHED: 12 of 12 words.
//
//      lwz     r11,8(r3)           this->array
//      cmplwi  cr6,r11,0
//      beq-    cr6,fail
//      lwz     r10,12(r11)         array->count
//      cmplw   cr6,r4,r10          UNSIGNED compare
//      bge-    cr6,fail
//      lwz     r11,8(r11)          array->items
//      rlwinm  r10,r4,2,0,29       i * 4
//      lwzx    r3,r10,r11          index in rA -- see below
//      blr
//  fail:
//      li      r3,0
//      blr
//
// `cmplw` is the unsigned compare, so the index is unsigned and one test
// covers both ends of the range. Both guards branch AWAY to the same
// `return 0`, so the fetch is the fall-through and is written first.
//
// ---------------------------------------------------------------------
// THE ANSWER: an AND-MASK ON THE INDEX FLIPS `lwzx` OPERAND ORDER.
//
// `a->items[i]` emits `lwzx r3,r11,r10` -- base in rA. `a->items[i & M]`
// emits `lwzx r3,r10,r11` -- index in rA -- and, when M keeps all thirty
// low bits, the scaling instruction is the SAME WORD either way, because
// the mask is absorbed into the rlwinm the shift already needed:
//
//      i * 4          rlwinm r10,r4,2,0,29     (i << 2) & 0xFFFFFFFC
//      (i & M) * 4    rlwinm r10,r4,2,0,29     identical when M & 0x3FFFFFFF
//                                              == 0x3FFFFFFF
//
// So the mask is INVISIBLE in the instruction stream and shows up only as
// the operand order of the load that consumes it. The mechanism is that
// MSVC matches `base + (index << scale)` as an addressing mode and puts the
// base in rA; a masked index does not match that pattern, the address falls
// back to a generic add, and the generic add puts the index in rA.
//
// Only four masks satisfy the constraint -- 0x3FFFFFFF, 0x7FFFFFFF,
// 0xBFFFFFFF and 0xFFFFFFFF -- and the last folds away to nothing, so the
// original cleared at least one of the top two bits and none of the rest.
// All three surviving masks, signed or unsigned, named constant or literal,
// and the shift pair `((u32)i << 2) >> 2` that means the same thing, give
// byte-identical code. Which of them was written cannot be recovered from
// the bytes; the mask is redundant here in any case, because the
// `(u32)i < count` guard has already rejected everything it would change.
//
// Narrower masks flip the order too and are DISTINGUISHABLE, because they
// leave their own bits in the rlwinm: `(u16)i` gives `rlwinm r10,r4,2,14,29`
// and `(u8)i` gives `2,22,29`. Neither is this function.
//
// Ruled out on the way, all still `lwzx r3,r11,r10`: the member-function
// form of the OUTER function (`Holder::GetAt`), which is the lever that
// worked for sub_826C0FC8 and does nothing here; unsigned and 64-bit index
// types; index-first subscript `i[a->items]`; naming the array, the items
// pointer or the index in a local, in either declaration order; byte
// pointer arithmetic with the offset written first or last; the base field
// declared as a u32 holding a pointer; `void* const*` elements; the ternary
// form; inverted guard polarity; and taking the element's address first.
// That is 30 shapes over three probe files, and the mask is the only thing
// that moved the word.

#include "types.h"

struct Array
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ void** items;
    /* 0x0C */ u32   count;
};

ASSERT_OFFSET(Array, items, 0x08);
ASSERT_OFFSET(Array, count, 0x0C);

struct Holder
{
    /* 0x00 */ char   unk0000[0x08];
    /* 0x08 */ Array* array;
};

ASSERT_OFFSET(Holder, array, 0x08);

void* GetAt(Holder* h, int i)
{
    if (h->array != 0 && (u32)i < h->array->count)
        return h->array->items[i & 0x3FFFFFFF];
    return 0;
}
