// sub_8264B718 -- read a byte out of the entry a global index selects.
// 32 bytes, 4 callers.
//
//      lis     r11,-32092
//      addi    r10,r11,1572       address of the global   -> 82A40624
//      lwz     r11,1572(r11)      the field at +0
//      lwz     r10,12(r10)        the field at +12
//      rlwinm  r9,r11,2,0,29      index * 4
//      lwzx    r8,r9,r10
//      lbz     r3,72(r8)
//      blr
//
// ONE symbol, not two. The +0 field folds its low half straight into the
// `lwz` displacement, which is what a relocated lis/lwz pair does; the +12
// field cannot, because MSVC will not combine a relocated immediate with a
// constant, so it materialises the address with an `addi` and puts 12 in the
// load. A second, distinct global would have brought its own `lis`.
//
// The +12 field is a pointer to an array of pointers -- the index is scaled
// by 4 and used to load a POINTER, which is then dereferenced at +72.
//
// `lbz` with no trailing `clrlwi r3,r3,24`: the return is u8, not bool.
//
// 3 of 8 words are relocated.

#include "types.h"

struct Ent72
{
    /* 0x00 */ char unk0000[0x48];
    /* 0x48 */ u8   state;
};

ASSERT_OFFSET(Ent72, state, 0x48);

struct EntTable
{
    /* 0x00 */ s32    current;
    /* 0x04 */ char   unk0004[0x08];
    /* 0x0C */ Ent72** entries;
};

ASSERT_OFFSET(EntTable, entries, 0x0C);

extern EntTable g_entTable;

u8 CurrentState()
{
    return g_entTable.entries[g_entTable.current]->state;
}
