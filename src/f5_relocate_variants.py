"""Source shapes for sub_8216E778, for tools/permute.py.

The first attempt is 108 B against 116 and the two missing words are both
COPIES the plain spelling does not produce:

    8216E77C  addi   r11,r3,12    an address that is never read
    8216E7AC  rotlwi r11,r11,0    a copy of the value just stored to p->kids

and with the second missing, `lbz r9,0(p)` HOISTS above `stw r11,4(p)`,
which is the exact symptom MATCHED.md's address-of lever describes. The
outer level already keeps `lhz r9,8(r3)` below its store, so whatever the
source does it does at one level and not the other -- unless the update is
an inlined helper taking a pointer, in which case it does it at all three.

Also under test: `add r11,r10,r4` (the loaded field in rA) where the plain
`t->nodes + delta` gives `add r11,r4,r11`.
"""

H = '#include "types.h"\n\n'

DECLS = """
struct Node
{
    u8  n;
    u8  unk0001[3];
    u32 kids;
};

struct Table
{
    u8  unk0000[8];
    u16 count;
    u8  unk000A[2];
    u32 nodes;
};
"""

BODIES = [
    ("plain, delta + field", H + DECLS + """
void RelocateChains(Table* t, int delta)
{
    t->nodes = delta + t->nodes;

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        p->kids = delta + p->kids;

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            q->kids = delta + q->kids;
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("helper taking u32*, d + *p", H + DECLS + """
static void Bump(u32* p, int d)
{
    *p = d + *p;
}

void RelocateChains(Table* t, int delta)
{
    Bump(&t->nodes, delta);

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        Bump(&p->kids, delta);

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            Bump(&q->kids, delta);
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("helper taking u32*, *p + d", H + DECLS + """
static void Bump(u32* p, int d)
{
    *p = *p + d;
}

void RelocateChains(Table* t, int delta)
{
    Bump(&t->nodes, delta);

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        Bump(&p->kids, delta);

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            Bump(&q->kids, delta);
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("local u32* at every site", H + DECLS + """
void RelocateChains(Table* t, int delta)
{
    u32* s = &t->nodes;
    *s = delta + *s;

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        u32* a = &p->kids;
        *a = delta + *a;

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            u32* b = &q->kids;
            *b = delta + *b;
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("u32& at every site", H + DECLS + """
void RelocateChains(Table* t, int delta)
{
    u32& s = t->nodes;
    s = delta + s;

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        u32& a = p->kids;
        a = delta + a;

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            u32& b = q->kids;
            b = delta + b;
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("two-level helper", H + DECLS + """
static void BumpWord(u32* p, int d)
{
    *p = d + *p;
}

static void BumpNode(Node* n, int d)
{
    BumpWord(&n->kids, d);
}

void RelocateChains(Table* t, int delta)
{
    BumpWord(&t->nodes, delta);

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        BumpNode(p, delta);

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            BumpNode(q, delta);
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("helper on the table only", H + DECLS + """
static void Bump(u32* p, int d)
{
    *p = d + *p;
}

void RelocateChains(Table* t, int delta)
{
    Bump(&t->nodes, delta);

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        p->kids = delta + p->kids;

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            q->kids = delta + q->kids;
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("helper, count read after the bump", H + DECLS + """
static void Bump(u32* p, int d)
{
    *p = d + *p;
}

void RelocateChains(Table* t, int delta)
{
    Bump(&t->nodes, delta);

    Node* p = (Node*)t->nodes;

    while (p != (Node*)t->nodes + t->count)
    {
        Bump(&p->kids, delta);

        Node* q = (Node*)p->kids;

        while (q != (Node*)p->kids + p->n)
        {
            Bump(&q->kids, delta);
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
    ("helper, field + d, plain q loop", H + DECLS + """
static void Bump(u32* p, int d)
{
    *p = d + *p;
}

void RelocateChains(Table* t, int delta)
{
    Bump(&t->nodes, delta);

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        Bump(&p->kids, delta);

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            q->kids = q->kids + delta;
            q = q + 1;
        }

        p = p + 1;
    }
}
"""),
]
