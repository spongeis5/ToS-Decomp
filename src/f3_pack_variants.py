"""Source shapes for sub_825FAC00, for tools/permute.py.

The first attempt is 16 of 26 with 2 words MISSING: the target carries a
`rotlwi rD,rS,0` copy of the just-stored field before each of the second and
third packs, and it computes the mask from that copy AFTER loading and
shifting the node byte, where the plain spelling computes the mask first
straight out of the store's own register.

    target:  stw r7,4(r3) ; rotlwi r6,r7,0 ; lbz ; extsb ; slwi
             ; rlwinm r9,r6,0,29,25 ; or r8,r10,r9 ; stw r8,4(r3)
    ours:    stw r7,4(r3) ; rlwinm r6,r7,0,29,25 ; lbz ; extsb ; slwi
             ; or r9,r10,r6 ; stw r9,4(r3)

So the question is what makes the field read a separate common subexpression
with its own register, and what puts the mask after the byte load.
"""

H = '#include "types.h"\n\n'

DECLS = """
struct Node
{
    u8 unk0000[25];
    s8 a;
    s8 b;
    s8 c;
};

struct Slot
{
    Node* node;
    int   bits;
    u32   extra;
};
"""

PRE = """
    o->bits  = (int)v[0];
    o->extra = v[1];
    o->node  = n;

    if (n == 0)
        return;
"""

BODIES = [
    ("field & mask | value", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    o->bits = (o->bits & ~7)     |  n->a;
    o->bits = (o->bits & ~0x38)  | (n->b << 3);
    o->bits = (o->bits & ~0x1C0) | (n->c << 6);
}
"""),
    ("value | field & mask", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    o->bits =  n->a        | (o->bits & ~7);
    o->bits = (n->b << 3)  | (o->bits & ~0x38);
    o->bits = (n->c << 6)  | (o->bits & ~0x1C0);
}
"""),
    ("through int* p, field first", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    int* p = &o->bits;
    *p = (*p & ~7)     |  n->a;
    *p = (*p & ~0x38)  | (n->b << 3);
    *p = (*p & ~0x1C0) | (n->c << 6);
}
"""),
    ("through int* p, value first", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    int* p = &o->bits;
    *p =  n->a        | (*p & ~7);
    *p = (n->b << 3)  | (*p & ~0x38);
    *p = (n->c << 6)  | (*p & ~0x1C0);
}
"""),
    ("through int& b", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    int& b = o->bits;
    b = (b & ~7)     |  n->a;
    b = (b & ~0x38)  | (n->b << 3);
    b = (b & ~0x1C0) | (n->c << 6);
}
"""),
    ("inlined helper returning the packed value", H + DECLS + """
static int Pack(int bits, int mask, int v)
{
    return (bits & mask) | v;
}

void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    o->bits = Pack(o->bits, ~7,     n->a);
    o->bits = Pack(o->bits, ~0x38,  n->b << 3);
    o->bits = Pack(o->bits, ~0x1C0, n->c << 6);
}
"""),
    ("inlined helper taking int*", H + DECLS + """
static void Pack(int* p, int mask, int v)
{
    *p = (*p & mask) | v;
}

void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    Pack(&o->bits, ~7,     n->a);
    Pack(&o->bits, ~0x38,  n->b << 3);
    Pack(&o->bits, ~0x1C0, n->c << 6);
}
"""),
    ("temp per statement", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    int b;
    b = o->bits;  o->bits = (b & ~7)     |  n->a;
    b = o->bits;  o->bits = (b & ~0x38)  | (n->b << 3);
    b = o->bits;  o->bits = (b & ~0x1C0) | (n->c << 6);
}
"""),
    ("positive hex masks", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    o->bits = (o->bits & 0xFFFFFFF8) |  n->a;
    o->bits = (o->bits & 0xFFFFFFC7) | (n->b << 3);
    o->bits = (o->bits & 0xFFFFFE3F) | (n->c << 6);
}
"""),
    ("u32 bits field", H + """
struct Node
{
    u8 unk0000[25];
    s8 a;
    s8 b;
    s8 c;
};

struct Slot
{
    Node* node;
    u32   bits;
    u32   extra;
};

void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    o->bits = (o->bits & ~7u)     |  n->a;
    o->bits = (o->bits & ~0x38u)  | (n->b << 3);
    o->bits = (o->bits & ~0x1C0u) | (n->c << 6);
}
"""),
    ("member function", H + DECLS.replace("struct Slot\n{", "struct Slot\n{\n    void Set(Node* n, const u32* v);") + """
void Slot::Set(Node* n, const u32* v)
{
    Slot* o = this;""" + PRE + """
    o->bits = (o->bits & ~7)     |  n->a;
    o->bits = (o->bits & ~0x38)  | (n->b << 3);
    o->bits = (o->bits & ~0x1C0) | (n->c << 6);
}
"""),
    ("shift written as a separate local", H + DECLS + """
void SlotSet(Slot* o, Node* n, const u32* v)
{""" + PRE + """
    int x;
    x = n->a;       o->bits = (o->bits & ~7)     | x;
    x = n->b << 3;  o->bits = (o->bits & ~0x38)  | x;
    x = n->c << 6;  o->bits = (o->bits & ~0x1C0) | x;
}
"""),
]
