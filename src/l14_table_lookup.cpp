// sub_8215BCF0 -- linear search of a four-entry global table. 80 B,
// 4 callers.
//
//      lis   r11,-32102 ; li r10,0 ; addi r9,r11,-1628      = 8299F9A4
//      mr    r11,r9                                          the walking base
// L:   lwz   r8,0(r11)
//      cmpw  cr6,r8,r3
//      beq-  cr6,found
//      addi  r11,r11,12          the stride IS the element size
//      addi  r8,r9,48            end = base + 4 * 12
//      addi  r10,r10,1
//      cmpw  cr6,r11,r8
//      blt+  cr6,L
//      li    r3,0 ; blr
// found:
//      rlwinm r11,r10,1,0,30 ; add r8,r10,r11 ; rlwinm r7,r8,2,0,29
//      addi   r9,r9,8
//      lwzx   r3,r7,r9
//      blr
//
// The loop carries BOTH an induction pointer and an index, and the index is
// live only after the loop -- MSVC strength-reduced a `for (i = 0; i < 4;
// i++)` and kept `i` because the returned expression is subscripted by it.
// The end compare is `cmpw`, SIGNED, on what are now pointers: that
// signedness is inherited from the original `int i < 4` and is why the index
// is an int rather than unsigned.
//
// `rlwinm ,1 ; add ; rlwinm ,2` is `i * 12` built as `(i + i*2) * 4`, the
// idiom-table form, so the element is 12 bytes and 48 is four of them.  The
// `+ 8` is folded onto the base rather than into the index because the base
// is a RELOCATED lis/addi pair, which never folds with anything.
//
// Both compares against the key are `cmpw`, so the key field and the
// parameter are signed.

#include "types.h"

struct TableEntry
{
    /* 0x00 */ s32 key;
    /* 0x04 */ s32 unk0004;
    /* 0x08 */ s32 value;
};
ASSERT_SIZE(TableEntry, 12);

extern TableEntry g_lookup_8299F9A4[4];

s32 LookupValue(s32 key)
{
    int i;

    for (i = 0; i < 4; i++)
        if (g_lookup_8299F9A4[i].key == key)
            return g_lookup_8299F9A4[i].value;

    return 0;
}
