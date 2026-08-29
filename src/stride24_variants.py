"""Source shapes for sub_826C0FC8, for tools/permute.py.

Ours has the right six instructions with r10 and r11 exactly swapped:

    target:  rlwinm r11,r4,1 ; lwz r10,24(r3) ; add r11,r4,r11 ; ...
    ours:    rlwinm r10,r4,1 ; lwz r11,24(r3) ; add r10,r4,r10 ; ...

The arithmetic chain lives in r11 for the target and r10 for us, and the
loaded base is the other way round. Register allocation, not semantics.
"""

H = '#include "types.h"\n\n'
DECL = """
struct E24 { char unk0000[24]; };
ASSERT_SIZE(E24, 24);
struct Holder24 { char unk0000[0x18]; E24* items; };
ASSERT_OFFSET(Holder24, items, 0x18);
"""

BODIES = [
    ("index into the member array", H + DECL + """
E24* At24(Holder24* h, int i) { return &h->items[i]; }
"""),
    ("pointer arithmetic on the member", H + DECL + """
E24* At24(Holder24* h, int i) { return h->items + i; }
"""),
    ("byte arithmetic with an explicit stride", H + DECL + """
E24* At24(Holder24* h, int i)
{
    return (E24*)((char*)h->items + i * 24);
}
"""),
    ("index computed into a local first", H + DECL + """
E24* At24(Holder24* h, int i)
{
    int off = i * 24;
    return (E24*)((char*)h->items + off);
}
"""),
    ("base loaded into a local first", H + DECL + """
E24* At24(Holder24* h, int i)
{
    E24* base = h->items;
    return &base[i];
}
"""),
    ("unsigned index", H + DECL + """
E24* At24(Holder24* h, unsigned i) { return &h->items[i]; }
"""),
    ("member function", H + """
struct E24 { char unk0000[24]; };
ASSERT_SIZE(E24, 24);
struct Holder24
{
    char unk0000[0x18];
    E24* items;
    E24* At(int i);
};
ASSERT_OFFSET(Holder24, items, 0x18);

E24* Holder24::At(int i) { return &items[i]; }
"""),
]
