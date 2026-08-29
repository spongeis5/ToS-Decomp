"""Source shapes for sub_8280D210, for tools/permute.py.

First attempt: 2 of 27 at /O2, 108 B against 112, and every difference is a
naming question rather than an instruction. The target does three things the
plain spelling does not:

    8280D240  lwz   r10,4(r3)     RELOADS c->index at the loop top
    8280D248  mulli r9,r10,48     multiplies rather than 3i<<4
    8280D264  lwz   r9,4(r3)      reloads c->index at the loop bottom
    8280D268  lwz   r10,0(r3)     and reloads c->owner there too, though
                                  r11 still holds it and is used at D244

That is +3 words for the reloads and -2 for the `mulli`, which is exactly the
one word we are short. /O2 /Os is NOT the answer -- it was measured and comes
out at 80 B, 2 of 20, having folded the whole guard into the loop.

The axis under test is therefore which occurrences of `c->owner` and
`c->index` are named in locals and which are spelled out, plus whether the
element address is written as a subscript or as byte arithmetic.
"""

H = '#include "types.h"\n\n'

DECLS = """
struct Entry
{
    int unk0000;
    int count;
    int state;
    u8  unk000C[0x24];
};

struct Owner
{
    Entry* entries;
};

struct Cursor
{
    Owner* owner;
    int    index;
};
"""

BODIES = [
    ("local o for guards, spelled out at the bottom", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("everything spelled out, if + do/while", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    if (c->index > c->owner->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)c->owner->entries->count)
        return;

    do
    {
        if (c->owner->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("everything spelled out, while", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    if (c->index > c->owner->entries->count)
        return;

    c->index = c->index + 1;

    while ((u32)c->index <= (u32)c->owner->entries->count)
    {
        if (c->owner->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
}
"""),
    ("local o everywhere, while", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    while ((u32)c->index <= (u32)o->entries->count)
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
}
"""),
    ("local o for guards and body, while spelled out", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    while ((u32)c->index <= (u32)c->owner->entries->count)
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
}
"""),
    ("local o for guards, spelled out in the loop", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (c->owner->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("byte arithmetic for the element", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (((Entry*)((char*)o->entries + c->index * 48))->state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("entries + index, pointer add", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if ((o->entries + c->index)->state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("index kept in a local, written back", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;
    int    i = c->index;

    if (i > o->entries->count)
        return;

    c->index = i + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("++ instead of = x + 1", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    ++c->index;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (o->entries[c->index].state != -2)
            return;
        ++c->index;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
    ("unsigned index field, one cast on the first test", H + """
struct Entry
{
    int unk0000;
    int count;
    int state;
    u8  unk000C[0x24];
};

struct Owner
{
    Entry* entries;
};

struct Cursor
{
    Owner* owner;
    u32    index;
};

void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if ((int)c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if (c->index > (u32)o->entries->count)
        return;

    do
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while (c->index <= (u32)c->owner->entries->count);
}
"""),
    ("inlined accessors on the cursor", H + DECLS.replace(
        "struct Cursor\n{\n    Owner* owner;\n    int    index;\n};",
        "struct Cursor\n{\n    Owner* owner;\n    int    index;\n"
        "    int Limit() const { return owner->entries->count; }\n"
        "    Entry* At() const { return &owner->entries[index]; }\n};") + """
void CursorSkipFree(Cursor* c)
{
    if (c->index > c->Limit())
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)c->Limit())
        return;

    do
    {
        if (c->At()->state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->Limit());
}
"""),
    ("guard as a nested positive if, while", H + DECLS + """
void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index <= o->entries->count)
    {
        c->index = c->index + 1;

        while ((u32)c->index <= (u32)c->owner->entries->count)
        {
            if (o->entries[c->index].state != -2)
                return;
            c->index = c->index + 1;
        }
    }
}
"""),
    ("volatile index", H + """
struct Entry
{
    int unk0000;
    int count;
    int state;
    u8  unk000C[0x24];
};

struct Owner
{
    Entry* entries;
};

struct Cursor
{
    Owner*       owner;
    volatile int index;
};

void CursorSkipFree(Cursor* c)
{
    Owner* o = c->owner;

    if (c->index > o->entries->count)
        return;

    c->index = c->index + 1;

    if ((u32)c->index > (u32)o->entries->count)
        return;

    do
    {
        if (o->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
    while ((u32)c->index <= (u32)c->owner->entries->count);
}
"""),
]
