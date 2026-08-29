// sub_8215BCB0 -- the key of the i'th entry of the global four-entry table.
// 28 B, 3 callers.
//
//      rlwinm r11,r3,1,0,30      i * 2
//      lis    r10,-32102
//      add    r11,r3,r11         i + i*2
//      addi   r8,r10,-1628       = 8299F9A4
//      rlwinm r9,r11,2,0,29      * 4  ->  i * 12
//      lwzx   r3,r9,r8
//
// The SAME table as sub_8215BCF0 (src/l14_table_lookup.cpp), 0x40 earlier in
// the image: same base address 8299F9A4, same `(i + i*2) * 4` twelve-byte
// stride, and this one returns the entry's first word where that one
// searches on it and returns the third.  The two are almost certainly one
// translation unit; they are kept in separate files because a wrong merge
// invents a type identity that compiles fine, while a wrong split only costs
// a duplicated declaration.
//
// No bounds check and no null test: the index is trusted.

#include "types.h"

struct TableEntry
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 unk0004;
    /* 0x08 */ s32 value;
};
ASSERT_SIZE(TableEntry, 12);

extern TableEntry g_lookup_8299F9A4[4];

s32 KeyAt(s32 i)
{
    return g_lookup_8299F9A4[i].key;
}
