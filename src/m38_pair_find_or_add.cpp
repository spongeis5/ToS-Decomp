// sub_82158850 -- look a byte key up in a small array of 2-byte pairs and
// append it if it is not there, returning the entry either way.
// 124 bytes, 4 callers.
//
//      lbz  r7,32(r3) ; mr r11,r3 ; li r10,0
//      cmpwi cr6,r7,0 ; ble- cr6,<after>
//      clrlwi r8,r4,24 ; mr r9,r3
//  L:  lbz r5,0(r9) ; cmplw cr6,r5,r8 ; beq- cr6,<after>
//      lbz r6,32(r11) ; addi r10,r10,1 ; addi r9,r9,2
//      cmpw cr6,r10,r6 ; blt+ cr6,L
//  after:
//      cmpw cr6,r10,r7 ; bne- cr6,<justreturn>
//      rlwinm r9,r10,1,0,30
//      rlwinm r10,r10,1,0,30          <- the SAME value, computed twice
//      add r9,r9,r11 ; li r8,0 ; add r3,r10,r11
//      stb r4,0(r9) ; stb r8,1(r9)
//      lbz r10,32(r11) ; addi r7,r10,1 ; stb r7,32(r11) ; blr
//  justreturn:
//      rlwinm r10,r10,1,0,30 ; add r3,r10,r11 ; blr
//
// `i * 2` is computed TWICE into two registers on the append path, which is
// the copied-common-subexpression fingerprint: the source spells the element
// out again for the return rather than naming it. Naming it would collapse
// the pair.
//
// The count is read three ways and all three are in the image: once at entry
// into r7, RELOADED at the loop latch into r6, and read again after the store.
// The latch reload is the ordinary rotated-`while` shape; the post-loop
// `cmpw r10,r7` has to use the entry value because the `break` exit does not
// pass through the latch, so one spelling -- `i < m->count` everywhere --
// produces all of it.
//
// The walk is strength-reduced to a pointer stepping by 2, so the element is
// two bytes and 32 is 16 of them; `stb` writes both halves, so both are bytes.
//
// `cmplw` on the keys after a `clrlwi ...,24` of the parameter: the compare
// needs the argument narrowed, the store at the end does not and uses the raw
// register.
//
// Nothing is relocated: 31 of 31 words are compared.

#include "types.h"

struct BytePair
{
    /* 0x00 */ u8 key;
    /* 0x01 */ u8 value;
};

ASSERT_SIZE(BytePair, 2);

struct PairMap
{
    /* 0x00 */ BytePair items[16];
    /* 0x20 */ u8       count;
};

ASSERT_OFFSET(PairMap, count, 0x20);

BytePair* FindOrAdd(PairMap* m, u8 key)
{
    int i;

    for (i = 0; i < m->count; i++)
    {
        if (m->items[i].key == key)
            break;
    }

    if (i == m->count)
    {
        m->items[i].key = key;
        m->items[i].value = 0;
        m->count = m->count + 1;
    }

    return &m->items[i];
}
