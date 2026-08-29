// sub_82772FC0 -- advance a playback cursor: if the cursor has run off its
// list, snap the position to a default; otherwise clamp forward to the
// current entry's start, add its length, and step the index. 152 B,
// 5 callers.  Built at /O2 /Os.
//
//      lwz     r10,24(r3)          index, folded to r3
//      addi    r11,r3,20           &cursor -- ONE base for 0(r11) and 4(r11)
//      cmpwi   cr6,r10,0 ; blt-    SIGNED   -> 1
//      lwz     r9,0(r11) ; lwz r9,4(r9)           list->count
//      cmplw   cr6,r10,r9 ; li r10,0 ; blt-       UNSIGNED -> 0
//      li      r10,1
//      clrlwi. r10,r10,24 ; bne-  -> the fallback arm, at the END
//
// Three things this function states, each read off the listing:
//
//   The materialised 0/1 followed by a redundant `clrlwi.` is MATCHED.md's
//   inlined-bool-helper fingerprint, and the signed `cmpwi` then unsigned
//   `cmplw` on the same value is the signedness split -- one predicate,
//   `index < 0 || (u32)index >= (u32)count`.
//
//   `bne-` jumps FORWARD to 82773048, so the fallback is written LAST and
//   the stepping path is the fall-through.  Branch polarity is source order.
//
//   `cmplw` on the position against the entry's start is UNSIGNED while
//   `cmpw` on the index against the count is SIGNED, so the position and the
//   start are u32 and the index and count are s32.
//
// The cursor base only appears when `&p->cursor` is named AFTER the guard:
// naming it before the guard gives 6 of 38 at /O2 and 8 of 37 at /Os, and
// spelling `p->cursor.x` at every use folds every access to r3 and never
// forms the base at all (4 of 38).  Seven shapes were measured; this one and
// a three-member Cursor are both 38 of 38, and only at /Os -- at /O2 the
// same source is 8 of 38 and twelve bytes too long, which is the
// register-coalescing signature.
//
// `mulli r9,r9,12` fixes the entry stride at 12 bytes; `lwz r9,4(r9)` on the
// list gives count at +4 and `lwz r8,0(r8)` the item pointer at +0.

#include "types.h"

struct Entry
{
    /* 0x00 */ u32 start;
    /* 0x04 */ u32 length;
    /* 0x08 */ s32 f08;
};
ASSERT_SIZE(Entry, 12);

struct EntryList
{
    /* 0x00 */ Entry* items;
    /* 0x04 */ s32    count;
};

struct Fallback
{
    /* 0x00 */ s32 f00;
    /* 0x04 */ u32 value;
};

struct Cursor
{
    /* 0x00 */ EntryList* list;
    /* 0x04 */ s32        index;

    bool AtEnd() const { return index < 0 || (u32)index >= (u32)list->count; }
};

struct Player
{
    /* 0x00 */ char      unk0000[20];
    /* 0x14 */ Cursor    cursor;
    /* 0x1C */ Fallback* fallback;
    /* 0x20 */ u32       pos;
};
ASSERT_OFFSET(Player, cursor,   0x14);
ASSERT_OFFSET(Player, fallback, 0x1C);
ASSERT_OFFSET(Player, pos,      0x20);

void CursorStep(Player* p)
{
    if (!p->cursor.AtEnd())
    {
        Cursor* c = &p->cursor;
        Entry* e = &c->list->items[c->index];
        if (p->pos < e->start)
        {
            p->pos = e->start;
            return;
        }
        p->pos = e->length + p->pos;
        if (c->index < c->list->count)
            c->index = c->index + 1;
        return;
    }
    p->pos = p->fallback->value;
}
