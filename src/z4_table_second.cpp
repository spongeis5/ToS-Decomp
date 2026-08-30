#include "types.h"

// sub_8215BCD0 -- the SECOND word of the i'th entry of the same global
// four-entry table that src/l32_table_key.cpp and src/l14_table_lookup.cpp
// index.  32 B.
//
//      rlwinm r11,r3,1,0,30      i * 2
//      lis    r10,-32102
//      add    r9,r3,r11          i + i*2
//      addi   r11,r10,-1628      = 8299F9A4, the table base
//      rlwinm r8,r9,2,0,29       * 4  ->  i * 12
//      addi   r7,r11,4           the FIELD offset, folded onto the base
//      lwzx   r3,r8,r7
//      blr
//
// Identical to sub_8215BCB0 (KeyAt) with one extra `addi`: `lis` + `addi` +
// a second `addi` is the idiom-table form for a field inside a global array
// element, and the second addi is the field offset -- 4 here, 8 in
// sub_8215BCF0's `.value`, 0 (absent) in sub_8215BCB0's `.key`.  The +4 lands
// on the base rather than in the index for the reason l14_table_lookup.cpp
// records: the base is a RELOCATED lis/addi pair and never folds with
// anything.
//
// `(i + i*2) * 4` is the 12-byte-stride idiom, so this is the same 12-byte
// TableEntry, and the index is trusted -- no bounds check, no null test.
//
// Index-first `lwzx r3,r8,r7` matches sub_8215BCB0's `lwzx r3,r9,r8`.

struct TableEntry
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 unk0004;
    /* 0x08 */ s32 value;
};
ASSERT_SIZE(TableEntry, 12);

extern TableEntry g_lookup_8299F9A4[4];

s32 SecondAt(s32 i)
{
    return g_lookup_8299F9A4[i].unk0004;
}
