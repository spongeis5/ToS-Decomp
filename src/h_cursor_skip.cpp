#include "types.h"

// sub_827C6D08 -- advance a 1-based cursor past every entry whose kind is -2,
// stopping at the count held in entry 0. 112 B, 14 callers.  /O2 /Os.
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
//      rlwinm  r9,r10,5,0,26    i * 32
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
// Three things, in the order they cost time.
//
// 1. The count sits at `entries[0] + 4`, INSIDE the first element of a
//    32-byte-stride array (`rlwinm ...,5,0,26`). So the array is 1-based and
//    element 0 is its header. Reading that as "the count is a sibling field"
//    puts it at a different offset and every displacement is wrong.
//
// 2. THE SIGNEDNESS SPLITS BETWEEN THE GUARD AND THE LOOP. Compare 1 is
//    `cmpw`, compares 2 and 3 are `cmplw`, on the same two values. With one
//    signedness throughout, all three come out the same kind. An unsigned
//    count with a `(s32)` cast on the guard gives the split; so would a
//    signed count with the loop cast, and the two are indistinguishable here.
//
// 3. IT IS AN /Os FUNCTION. At /O2 this source emits the same 28 instructions
//    in the same order and differs only by a systematic register renaming --
//    owner in r8 and index in r11 instead of r11 and r10. That is the
//    register-coalescing signature in MATCHED.md and it is the whole diff:
//    7 of 28 at /O2, 28 of 28 at /O2 /Os. Do not go looking for a source
//    shape when the instruction stream already agrees.
//
// The loop is a plain rotated `while`: the peeled copy of the bound test sits
// ahead of the loop and a second copy closes it, so it is not a do/while.

struct SkipEntry
{
    /* 0x00 */ s32  unk0000;
    /* 0x04 */ u32  count;      /* only meaningful in element 0 */
    /* 0x08 */ s32  kind;
    /* 0x0C */ char unk000C[0x14];
};
ASSERT_OFFSET(SkipEntry, count, 0x04);
ASSERT_OFFSET(SkipEntry, kind,  0x08);
ASSERT_SIZE(SkipEntry, 32);

struct SkipOwner
{
    /* 0x00 */ SkipEntry* entries;
};

struct SkipCursor
{
    /* 0x00 */ SkipOwner* owner;
    /* 0x04 */ s32        index;
};
ASSERT_OFFSET(SkipCursor, index, 0x04);

void CursorSkipDead(SkipCursor* c)
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
