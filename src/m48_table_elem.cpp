// sub_822D2510 -- address of a whole element of the same global array
// src/table_index.cpp and src/m21_table_byte.cpp index. 20 bytes, 3 callers.
//
//      lis     r11,-32099
//      mulli   r10,r3,1856
//      addi    r11,r11,-21312      -> 829CACC0
//      add     r3,r10,r11
//      blr
//
// One `addi` and not two, which is the whole difference from sub_822D2450:
// there a second `addi` carries a field offset inside the element, so the
// source takes the address of a MEMBER; here there is none, so it is the
// address of the element itself.
//
// Third function on the same table, and all three agree on base 829CACC0 and
// stride 1856.
//
// The lis and the addi are relocated, so 3 of 5 words are compared.

#include "types.h"

struct TElem
{
    /* 0x0000 */ u8 unk0000[1856];
};

// Not a guess: `mulli r10,r3,1856` in the target states the stride, and two
// other functions reach the same number from the same base.
ASSERT_SIZE(TElem, 1856);

extern TElem g_elems[];

TElem* ElemAt(int i)
{
    return &g_elems[i];
}
