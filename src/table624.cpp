#include "types.h"

// sub_822D2528 -- field of a global array element, stride 1856. 24 B, 5 callers.
//   lis r10,-32099 ; mulli r11,r3,1856 ; addi r10,r10,-21312
//   addi r10,r10,624 ; add r3,r11,r10 ; blr
// The same table as sub_822D2450 (src/table_index.cpp) at a different field:
// 624 rather than 1248. Same stride, same base symbol.
struct Entry624
{
    char unk0000[624];
    s32  field;
    char unk0274[1856 - 624 - 4];
};
ASSERT_OFFSET(Entry624, field, 624);
ASSERT_SIZE(Entry624, 1856);
extern Entry624 g_table624[];
int* FieldOf624(int i) { return &g_table624[i].field; }
