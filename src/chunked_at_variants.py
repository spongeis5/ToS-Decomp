"""Source shapes for sub_82806FD0, for tools/permute.py.

The tail (6 instructions) and the head already match. The open question is one
scheduling decision: the target presets `r3 = 0` as its SECOND instruction,
which forces `this` into r10 and defers the base load, and turns the bounds
check into a conditional return (`bgtlr`) rather than a forward branch to a
`li r3,0; blr` epilogue.
"""

DECL = """
struct Map
{
    char  pad00[8];
    char* p08;
    char* p0C;
    char  pad10[8];
    char** chunks;
    int   nchunks;
};

struct Chunked
{
    char pad00[0x18];
    Map* map;
    char pad1C[4];
    unsigned base;
};

#define TOTAL(m) (((unsigned)((m)->nchunks - 1) << 5) \\
                  + (unsigned)(((m)->p08 - (m)->p0C) >> 4))
"""

BODIES = [
    ("guard clause, i before the test", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = self->base - k;
    if (i > total) return 0;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
"""),

    ("ternary", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = self->base - k;
    return i > total ? 0 : m->chunks[i >> 5] + (i & 31) * 16;
}
"""),

    ("single exit, result variable", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = self->base - k;
    char* r = 0;
    if (i <= total)
        r = m->chunks[i >> 5] + (i & 31) * 16;
    return r;
}
"""),

    ("test inverted, positive path first", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = self->base - k;
    if (i <= total)
        return m->chunks[i >> 5] + (i & 31) * 16;
    return 0;
}
"""),

    ("i computed after the guard reads base", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    if (self->base - k > total) return 0;
    unsigned i = self->base - k;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
"""),

    ("signed index", DECL + """
void* f(Chunked* self, int k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = (unsigned)((int)self->base - k);
    if (i > total) return 0;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
"""),

    ("returns char*, not void*", DECL + """
char* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned total = TOTAL(m);
    unsigned i = self->base - k;
    if (i > total) return 0;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
"""),

    ("total inlined into the compare", DECL + """
void* f(Chunked* self, unsigned k)
{
    Map* m = self->map;
    unsigned i = self->base - k;
    if (i > TOTAL(m)) return 0;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
"""),
]

# The aliasing hypothesis, kept because it was TESTED AND REFUTED rather than
# untried. With Map* and Chunked* as distinct types, MSVC's type-based alias
# analysis may reorder the load of self->base above the loads from *m. The
# target does not reorder -- it saves `this` in r10 and loads base late --
# so reading everything through ONE pointer type looked like the answer.
# It scores 10/21, worse than the 11/21 baseline.
BODIES.append(("everything through one pointer type", """
typedef unsigned u32;

void* ChunkedAt(char* self, u32 k)
{
    char* m = *(char**)(self + 0x18);
    u32 n = *(u32*)(m + 0x1C);
    char* p08 = *(char**)(m + 0x08);
    char* p0C = *(char**)(m + 0x0C);
    u32 total = ((n - 1) << 5) + (u32)((p08 - p0C) >> 4);
    u32 i = *(u32*)(self + 0x20) - k;
    if (i > total) return 0;
    return *(char**)(*(char**)(m + 0x18) + (i >> 5) * 4) + (i & 31) * 16;
}
"""))
