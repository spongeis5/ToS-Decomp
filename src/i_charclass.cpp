#include "types.h"

// sub_82692590 -- two-stage table lookup on a 16-bit character. 88 B, 13
// callers.
//
//      rlwinm  r11,r4,25,23,30    ((c >> 8) & 0xFF) * 2
//      clrlwi  r10,r4,16          c & 0xFFFF
//      lhzx    r11,r11,r3         v = t[c >> 8]
//      cmplwi  r11,0
//      bne-    +8
//      li      r3,0 ; blr         v == 0: no
//      cmplwi  cr6,r11,1
//      bne-    cr6,+8
//      li      r3,1 ; blr         v == 1: yes
//      rlwinm  r9,r10,28,28,31    (c >> 4) & 15
//      clrlwi  r10,r10,28         c & 15
//      add     r11,r9,r11         v + ((c >> 4) & 15)
//      li      r9,1
//      rlwinm  r11,r11,1,0,30     * 2
//      slw     r10,r9,r10         1 << (c & 15)
//      lhzx    r11,r11,r3         t[...]
//      and     r11,r10,r11
//      addic   r10,r11,-1
//      subfe   r3,r10,r11         != 0
//      blr
//
// The scale on both indices is 2 and the loads are lhzx, so the table is a
// u16 array; 0 and 1 are reserved answers and anything else is the base of a
// 16-entry row of bitmasks. That is the standard two-level trie for a
// character-property table.
//
// `clrlwi r10,r4,16` -- and the fact that the first index is masked to 8 bits
// rather than being a plain shift -- says the argument is a 16-bit unsigned,
// zero-extended by the callee.
//
// lhzx puts the scaled INDEX in rA and the base in rB, which per the lwzx
// note is the free-function `t[i]` form with the array at offset 0, i.e. a
// bare pointer parameter.
//
// `addic rD,rS,-1 ; subfe rT,rD,rS` is the branchless `!= 0`.
int CharHasProperty(const u16* t, u16 c)
{
    u32 v = t[c >> 8];

    if (v == 0)
        return 0;
    if (v == 1)
        return 1;

    return (t[v + ((c >> 4) & 15)] & (1u << (c & 15))) != 0;
}
