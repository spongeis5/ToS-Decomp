#include "types.h"

// sub_8215BD60 -- look a key up in a four-entry global table and report
// whether the caller's value agrees with the stored one. 88 B, 6 callers.
//
//      lis     r11,-32102
//      li      r10,0            i = 0
//      addi    r9,r11,-1628     r9 = g_table   (0x8299F9A4)
//      mr      r11,r9
//  L:  lwz     r8,0(r11)        g_table[i].key
//      cmpw    cr6,r8,r3        SIGNED
//      beq-    cr6,found
//      addi    r11,r11,12
//      addi    r8,r9,48         end = g_table + 4, recomputed each pass
//      addi    r10,r10,1
//      cmpw    cr6,r11,r8
//      blt+    cr6,L
//      cntlzw  r11,r4
//      rlwinm  r3,r11,27,31,31  return value == 0
//      blr
// found:mulli  r11,r10,12       i * 12, recomputed from the index
//      addi    r10,r9,4
//      lwzx    r9,r11,r10       g_table[i].value
//      subf    r8,r9,r4
//      cntlzw  r7,r8
//      rlwinm  r3,r7,27,31,31   return g_table[i].value == value
//      blr
//
// 48 bytes of span at a 12-byte stride is four entries, so the bound is read
// off the code and not guessed.
//
// BOTH returns are the branchless `cntlzw` + `rlwinm ...,27,31,31` form from
// the idiom table -- `x == 0` without the leading `addi -1`. The operand order
// of the `subf` says which side is which: MSVC emits `a == b` as
// `subf rD,rA,rB` computing rB - rA, and here rA is the loaded field and rB is
// the parameter, so the source reads `g_table[i].value == value`.
//
// `cmpw` rather than `cmplw` on the key makes both the field and the parameter
// signed.
//
// The hit block recomputes i * 12 with a `mulli` instead of reusing the
// walking pointer, which is the same strength-reduction split as
// src/f_name_lookup.cpp.

struct KeyValue
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 value;
    /* 0x08 */ s32 unk0008;
};
ASSERT_OFFSET(KeyValue, value, 0x04);
ASSERT_SIZE(KeyValue, 12);

extern KeyValue g_table[4];

bool TableAgrees(int key, int value)
{
    for (int i = 0; i < 4; ++i)
        if (g_table[i].key == key)
            return g_table[i].value == value;

    return value == 0;
}
