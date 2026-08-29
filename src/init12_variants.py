"""Source shapes for sub_826C1480, for tools/permute.py.

The store ORDER is already exactly right and 72 flag combinations all give the
same 13/19. What differs is one scheduling decision:

    target:  5 loads from the parameter home area, THEN stw r6,8(r3)
    ours:    stw r6,8(r3), THEN the same 5 loads

The target moved loads ABOVE a store to memory. A compiler will only do that
if it can prove the store cannot alias what the loads read. The loads read the
incoming parameter home area, which lives in the CALLER's frame at r1+84.. --
so a `S*` argument could in principle point at it, and a conservative compiler
must keep program order. Ours does. The retail one did not.

So the hypothesis under test is NOT scheduling policy but ALIAS INFORMATION:
what about the original declaration told the compiler these cannot overlap?

Every variant below stores in the same order, which is already correct; they
differ only in what the compiler is allowed to assume about the pointer.
"""

DECL = """
struct S { int f[12]; };
"""

BODY = """
    s->f[3] = d;  s->f[4] = e;  s->f[5]  = f;
    s->f[2] = c;
    s->f[0] = a;  s->f[1] = b;
    s->f[6] = g;  s->f[7] = h;  s->f[8]  = i;
    s->f[9] = j;  s->f[10] = k; s->f[11] = l;
"""

ARGS = ("int a, int b, int c, int unused, int d, int e, int f, "
        "int g, int h, int i, int j, int k, int l")

BODIES = [
    ("baseline: plain S* pointer", DECL + """
__declspec(dllexport) void Init12(S* s, %s)
{%s}
""" % (ARGS, BODY)),

    ("__restrict on the pointer", DECL + """
__declspec(dllexport) void Init12(S* __restrict s, %s)
{%s}
""" % (ARGS, BODY)),

    ("__declspec(noalias) on the function", DECL + """
__declspec(dllexport) __declspec(noalias) void Init12(S* s, %s)
{%s}
""" % (ARGS, BODY)),

    ("reference parameter", DECL + """
__declspec(dllexport) void Init12(S& r, %s)
{
    S* s = &r;
%s}
""" % (ARGS, BODY)),

    ("non-static member function", DECL.replace("struct S { int f[12]; };", """
struct S
{
    int f[12];
    void Init12(%s);
};
""" % ARGS) + """
void S::Init12(%s)
{
    S* s = this;
%s}
""" % (ARGS, BODY)),

    ("named fields, not an array", """
struct S
{
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11;
};

__declspec(dllexport) void Init12(S* s, %s)
{
    s->f3 = d;  s->f4 = e;  s->f5  = f;
    s->f2 = c;
    s->f0 = a;  s->f1 = b;
    s->f6 = g;  s->f7 = h;  s->f8  = i;
    s->f9 = j;  s->f10 = k; s->f11 = l;
}
""" % ARGS),

    ("fields written through a local int*", DECL + """
__declspec(dllexport) void Init12(S* s, %s)
{
    int* p = s->f;
    p[3] = d;  p[4] = e;  p[5]  = f;
    p[2] = c;
    p[0] = a;  p[1] = b;
    p[6] = g;  p[7] = h;  p[8]  = i;
    p[9] = j;  p[10] = k; p[11] = l;
}
""" % ARGS),

    ("bare int* parameter, no struct", """
__declspec(dllexport) void Init12(int* s, %s)
{
    s[3] = d;  s[4] = e;  s[5]  = f;
    s[2] = c;
    s[0] = a;  s[1] = b;
    s[6] = g;  s[7] = h;  s[8]  = i;
    s[9] = j;  s[10] = k; s[11] = l;
}
""" % ARGS),

    ("__restrict int* parameter", """
__declspec(dllexport) void Init12(int* __restrict s, %s)
{
    s[3] = d;  s[4] = e;  s[5]  = f;
    s[2] = c;
    s[0] = a;  s[1] = b;
    s[6] = g;  s[7] = h;  s[8]  = i;
    s[9] = j;  s[10] = k; s[11] = l;
}
""" % ARGS),

    ("unsigned fields and unsigned parameters", """
struct S { unsigned f[12]; };

__declspec(dllexport) void Init12(S* s, unsigned a, unsigned b, unsigned c,
    unsigned unused, unsigned d, unsigned e, unsigned f, unsigned g,
    unsigned h, unsigned i, unsigned j, unsigned k, unsigned l)
{%s}
""" % BODY),

    ("float fields reinterpreted -- distinct type for TBAA", """
struct S { float f[12]; };

__declspec(dllexport) void Init12(S* s, int a, int b, int c, int unused,
    int d, int e, int f, int g, int h, int i, int j, int k, int l)
{
    int* p = (int*)s->f;
    p[3] = d;  p[4] = e;  p[5]  = f;
    p[2] = c;
    p[0] = a;  p[1] = b;
    p[6] = g;  p[7] = h;  p[8]  = i;
    p[9] = j;  p[10] = k; p[11] = l;
}
"""),

    ("stores grouped as the target's register lifetimes suggest", DECL + """
__declspec(dllexport) void Init12(S* s, %s)
{
    s->f[3] = d;  s->f[4] = e;  s->f[5] = f;
    s->f[6] = g;  s->f[7] = h;  s->f[8] = i;  s->f[9] = j;  s->f[10] = k;
    s->f[2] = c;
    s->f[11] = l;
    s->f[0] = a;  s->f[1] = b;
}
""" % ARGS),
]
