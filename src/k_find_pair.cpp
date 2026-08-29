#include "types.h"

// sub_827DAC60 -- linear search of an 8-byte key/value array. 76 B, 11 callers.
//
//      lwz     r9,100(r3)     count
//      li      r11,0          i
//      cmplwi  cr6,r9,0
//      beq-    cr6,none
//      lwz     r8,96(r3)      items
//      mr      r10,r8
//  L:  lwz     r7,0(r10)
//      cmpw    cr6,r7,r4
//      beq-    cr6,hit
//      addi    r11,r11,1
//      addi    r10,r10,8
//      cmplw   cr6,r11,r9
//      blt+    cr6,L
// none:li      r3,0
//      blr
// hit: rlwinm  r11,r11,3,0,28   i * 8
//      add     r11,r11,r8
//      lwz     r3,4(r11)
//      blr
//
// The hit path RECOMPUTES `items + i*8` although r10 already holds it, which
// is the tell that the source subscripts twice: the walking pointer is the
// compiler's strength reduction of `items[i].key`, and `items[i].value` is a
// separate subscript that it does not fold back onto the induction variable.
//
// `cmplwi`/`cmplw` on the count and index against `cmpw` on the key: the
// count and index are unsigned, the key is signed.
//
// NEEDS /O2 /Os, and the difference is a new flavour of the /Os signature.
// It is not "fresh register versus reused source" this time -- at plain /O2
// the instructions and their order are identical and the two loop-carried
// values are simply TRANSPOSED: the index gets r10 and the walking pointer
// r11, where the target has the index in r11 and the pointer in r10. Seven
// words, all of them a register name. So when a loop with BOTH an index and
// an induction pointer comes out with the two swapped, try the flag before
// rewriting the loop.
struct Pair
{
    /* 0x00 */ s32   key;
    /* 0x04 */ void* value;
};
ASSERT_OFFSET(Pair, value, 0x04);

struct PairTable
{
    /* 0x00 */ char  unk0000[0x60];
    /* 0x60 */ Pair* items;
    /* 0x64 */ u32   count;

    void* Find(s32 key);
};
ASSERT_OFFSET(PairTable, items, 0x60);
ASSERT_OFFSET(PairTable, count, 0x64);

void* PairTable::Find(s32 key)
{
    for (u32 i = 0; i < count; ++i)
        if (items[i].key == key)
            return items[i].value;
    return 0;
}
