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
//
// NOT MATCHED, and the two optimisation levels each supply half of it.
//
//   /O2      13 of 22 words, 96 bytes. The loop and the miss return are
//            exact. The hit block expands i * 12 as the shift-add chain
//            `rlwinm 1 / add / rlwinm 2` -- three words where the target has
//            one `mulli r11,r10,12` -- so the block is two words long and
//            everything after it shifts.
//
//   /O2 /Os  9 of 21 words, 84 bytes. `mulli r11,r9,12` IS EMITTED, which is
//            the /Os instruction-selection signature already recorded for
//            the 12-byte stride in src/t4_span_dispatch.cpp. But /Os then
//            coalesces both `cntlzw` results into r11, which makes the two
//            returns' tails textually identical, and MSVC MERGES them: the
//            hit path ends in `b` to the miss path's `rlwinm r3,r11,27,31,31`
//            instead of carrying its own copy. The target has both copies,
//            with different source registers (r4 and r7), so it cannot have
//            been compiled that way.
//
// The register coalescing that /Os does is therefore what ENABLES the tail
// merge -- two returns are mergeable only once their inputs share a register
// -- so the size difference and the register difference are one phenomenon,
// not two.
//
// tools/flagsweep.py: 72 combinations, exactly TWO outcomes, 44 giving 13/57
// and 28 giving 9/57. There is no third setting between them.
//
// This is the third function in this batch that wants /O2's register
// allocation with /Os's instruction selection -- sub_826973C8 (cr6 from /O2,
// GPRs from /Os) and sub_821FC4D8 (fresh-register float chain from /O2, phi
// copy from /Os) are the others. Three independent instances say the split
// is a property of the toolchain configuration and not of any one function's
// source, and none of the 72 flag combinations expresses it.

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
