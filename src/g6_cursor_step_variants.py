"""Source shapes for sub_82772FC0.

Round 1 (1 of 38) established three separate errors, all readable off the
listing and all fixed here; what these shapes vary is only the third.

  * BRANCH POLARITY.  `bne- 0x82773048` jumps FORWARD to the fallback arm,
    so the fallback is written LAST and the stepping path is the
    fall-through.  Round 1 wrote the fallback first.
  * SIGNEDNESS.  `cmplw` on the position against the entry start is
    unsigned; `cmpw` on the index against the count is signed.  Round 1 made
    both signed.
  * THE CURSOR BASE.  The target forms `addi r11,r3,20` once and reads
    0(r11) and 4(r11) four times between them, while the first index read is
    still folded to 24(r3).  Round 1 folded all of them to r3, so the base
    never appeared.  That is what these variants are for.
"""

H = '#include "types.h"\n\n'

TYPES = """
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
"""

PLAYER = """
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
"""

CUR_PLAIN = """
struct Cursor
{
    /* 0x00 */ EntryList* list;
    /* 0x04 */ s32        index;
};
"""

CUR_ATEND = """
struct Cursor
{
    /* 0x00 */ EntryList* list;
    /* 0x04 */ s32        index;

    bool AtEnd() const { return index < 0 || (u32)index >= (u32)list->count; }
};
"""

CUR_ALL = """
struct Cursor
{
    /* 0x00 */ EntryList* list;
    /* 0x04 */ s32        index;

    bool AtEnd() const { return index < 0 || (u32)index >= (u32)list->count; }
    Entry* Current() const { return &list->items[index]; }
    void Next() { if (index < list->count) index = index + 1; }
};
"""

BODIES = [
    ("member AtEnd, fields spelled out",
     H + TYPES + CUR_ATEND + PLAYER + """
void CursorStep(Player* p)
{
    if (!p->cursor.AtEnd())
    {
        Entry* e = &p->cursor.list->items[p->cursor.index];
        if (p->pos < e->start)
        {
            p->pos = e->start;
            return;
        }
        p->pos = e->length + p->pos;
        if (p->cursor.index < p->cursor.list->count)
            p->cursor.index = p->cursor.index + 1;
        return;
    }
    p->pos = p->fallback->value;
}
"""),
    ("member AtEnd, Cursor* local",
     H + TYPES + CUR_ATEND + PLAYER + """
void CursorStep(Player* p)
{
    Cursor* c = &p->cursor;
    if (!c->AtEnd())
    {
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
"""),
    ("member AtEnd on p->cursor, Cursor* local after",
     H + TYPES + CUR_ATEND + PLAYER + """
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
"""),
    ("three Cursor members",
     H + TYPES + CUR_ALL + PLAYER + """
void CursorStep(Player* p)
{
    if (!p->cursor.AtEnd())
    {
        Entry* e = p->cursor.Current();
        if (p->pos < e->start)
        {
            p->pos = e->start;
            return;
        }
        p->pos = e->length + p->pos;
        p->cursor.Next();
        return;
    }
    p->pos = p->fallback->value;
}
"""),
    ("free helper taking Cursor*",
     H + TYPES + CUR_PLAIN + PLAYER + """
static bool AtEnd(const Cursor* c)
{
    return c->index < 0 || (u32)c->index >= (u32)c->list->count;
}

void CursorStep(Player* p)
{
    if (!AtEnd(&p->cursor))
    {
        Entry* e = &p->cursor.list->items[p->cursor.index];
        if (p->pos < e->start)
        {
            p->pos = e->start;
            return;
        }
        p->pos = e->length + p->pos;
        if (p->cursor.index < p->cursor.list->count)
            p->cursor.index = p->cursor.index + 1;
        return;
    }
    p->pos = p->fallback->value;
}
"""),
    ("member AtEnd, if/else",
     H + TYPES + CUR_ATEND + PLAYER + """
void CursorStep(Player* p)
{
    if (!p->cursor.AtEnd())
    {
        Entry* e = &p->cursor.list->items[p->cursor.index];
        if (p->pos < e->start)
        {
            p->pos = e->start;
            return;
        }
        p->pos = e->length + p->pos;
        if (p->cursor.index < p->cursor.list->count)
            p->cursor.index = p->cursor.index + 1;
    }
    else
    {
        p->pos = p->fallback->value;
    }
}
"""),
    ("member AtEnd on Player",
     H + TYPES + CUR_PLAIN + """
struct Player
{
    /* 0x00 */ char      unk0000[20];
    /* 0x14 */ Cursor    cursor;
    /* 0x1C */ Fallback* fallback;
    /* 0x20 */ u32       pos;

    bool AtEnd() const
    {
        return cursor.index < 0 || (u32)cursor.index >= (u32)cursor.list->count;
    }
    void Step();
};
ASSERT_OFFSET(Player, cursor,   0x14);
ASSERT_OFFSET(Player, fallback, 0x1C);
ASSERT_OFFSET(Player, pos,      0x20);

void Player::Step()
{
    if (!AtEnd())
    {
        Entry* e = &cursor.list->items[cursor.index];
        if (pos < e->start)
        {
            pos = e->start;
            return;
        }
        pos = e->length + pos;
        if (cursor.index < cursor.list->count)
            cursor.index = cursor.index + 1;
        return;
    }
    pos = fallback->value;
}
"""),
]
