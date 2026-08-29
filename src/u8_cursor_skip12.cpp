#include "types.h"

// sub_8276DDC0 -- advance a 1-based cursor past every entry whose kind is -2,
// stopping at the count held in entry 0. 112 B, 6 callers.
//
// The SAME FUNCTION as sub_827C6D08 (src/h_cursor_skip.cpp), 112 bytes and 28
// instructions in the same order, over a 12-byte element instead of a 32-byte
// one -- so the shift becomes a `mulli`.
//
//      lwz     r11,0(r3)        a = c->owner
//      lwz     r10,4(r3)        i = c->index
//      lwz     r9,0(r11)        a->entries
//      lwz     r9,4(r9)         entries[0].count
//      cmpw    cr6,r10,r9       SIGNED
//      bgtlr   cr6
//      addi    r10,r10,1
//      stw     r10,4(r3)
//      lwz     r9,0(r11)
//      lwz     r9,4(r9)
//      cmplw   cr6,r10,r9       UNSIGNED
//      bgtlr   cr6
//  L:  lwz     r10,4(r3)
//      lwz     r8,0(r11)
//      mulli   r9,r10,12        i * 12
//      add     r9,r9,r8
//      lwz     r9,8(r9)         entries[i].kind
//      cmpwi   cr6,r9,-2
//      bnelr   cr6
//      addi    r10,r10,1
//      stw     r10,4(r3)
//      lwz     r9,4(r3)
//      lwz     r10,0(r3)
//      lwz     r10,0(r10)
//      lwz     r10,4(r10)
//      cmplw   cr6,r9,r10
//      ble+    cr6,L
//      blr
//
// Three things, all of them already paid for on the 32-byte twin.
//
// 1. The count sits at `entries[0] + 4`, INSIDE the first element of the
//    12-byte-stride array. The array is 1-based and element 0 is its header;
//    reading the count as a sibling field of `entries` puts every
//    displacement somewhere else.
//
// 2. THE SIGNEDNESS SPLITS between the guard and the loop -- compare 1 is
//    `cmpw`, compares 2 and 3 are `cmplw` on the same two values. One
//    signedness throughout makes all three the same kind. An unsigned count
//    with an (s32) cast on the guard produces the split.
//
// 3. The loop is a rotated `while`, not a do/while: the peeled bound test sits
//    ahead of it and a second copy closes it.

struct SkipEntry12
{
    /* 0x00 */ s32 unk0000;
    /* 0x04 */ u32 count;      /* only meaningful in element 0 */
    /* 0x08 */ s32 kind;
};
ASSERT_OFFSET(SkipEntry12, count, 0x04);
ASSERT_OFFSET(SkipEntry12, kind,  0x08);
ASSERT_SIZE(SkipEntry12, 12);

struct SkipOwner12
{
    /* 0x00 */ SkipEntry12* entries;
};

struct SkipCursor12
{
    /* 0x00 */ SkipOwner12* owner;
    /* 0x04 */ s32          index;
};
ASSERT_OFFSET(SkipCursor12, index, 0x04);

void CursorSkipDead12(SkipCursor12* c)
{
    if (c->index > (s32)c->owner->entries[0].count)
        return;

    ++c->index;

    while (c->index <= c->owner->entries[0].count)
    {
        if (c->owner->entries[c->index].kind != -2)
            return;
        ++c->index;
    }
}
