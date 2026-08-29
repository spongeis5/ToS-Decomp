"""Source shapes for sub_825A36C0, for tools/permute.py.

26 of 32 on the first attempt, with the only two real differences being the
ASSOCIATION of one product -- not the operand slots, which /fp:fast makes
unreadable (MATCHED.md).

    target:  fmuls f8,f9,f0   ((float)n * 0.4f)     then  fmuls f0,f8,f12  (* scale)
    ours:    fmuls f8,f12,f9  (scale * (float)n)    then  fmuls f0,f8,f0   (* 0.4f)

`(float)n * 0.4f * r->scale` associates left in the source and MSVC still
sank the constant to the far end, so the question is which spelling keeps
the constant in the FIRST product.
"""

H = '#include "types.h"\n\n'

DECLS = """
struct Source
{
    u8  unk0000[0x108];
    int total;
};

struct Rate
{
    u8      unk0000[0x20];
    Source* src;
    u8      unk0024[0x4C0];
    int     step;
    u8      unk04E8[0x374];
    f32     scale;
    u8      unk0860[0x1C];
    int     count;
};
"""

HEAD = """
int RateFromScale(Rate* r, int n)
{
    if (n < 1)
        n = 1;

    r->count = n;
"""

TAIL = """
    if (period < 0.01f)
        return 0;

    r->step = (int)((float)r->src->total / period);
    return 0;
}
"""

BODIES = [
    ("n * 0.4f * scale", H + DECLS + HEAD + """
    float period = (float)n * 0.4f * r->scale;
""" + TAIL),
    ("(n * 0.4f) * scale, parenthesised", H + DECLS + HEAD + """
    float period = ((float)n * 0.4f) * r->scale;
""" + TAIL),
    ("scale * (n * 0.4f)", H + DECLS + HEAD + """
    float period = r->scale * ((float)n * 0.4f);
""" + TAIL),
    ("temp for the first product", H + DECLS + HEAD + """
    float t = (float)n * 0.4f;
    float period = t * r->scale;
""" + TAIL),
    ("temp, 0.4f first", H + DECLS + HEAD + """
    float t = 0.4f * (float)n;
    float period = t * r->scale;
""" + TAIL),
    ("temp, scale second, reassigned", H + DECLS + HEAD + """
    float period = (float)n * 0.4f;
    period = period * r->scale;
""" + TAIL),
    ("temp, scale second, *=", H + DECLS + HEAD + """
    float period = (float)n * 0.4f;
    period *= r->scale;
""" + TAIL),
    ("float fn local", H + DECLS + HEAD + """
    float fn = (float)n;
    float period = fn * 0.4f * r->scale;
""" + TAIL),
    ("float fn local, temp product", H + DECLS + HEAD + """
    float fn = (float)n;
    float t = fn * 0.4f;
    float period = t * r->scale;
""" + TAIL),
    ("scale named first", H + DECLS + HEAD + """
    float s = r->scale;
    float period = (float)n * 0.4f * s;
""" + TAIL),
    ("scale named first, temp product", H + DECLS + HEAD + """
    float s = r->scale;
    float t = (float)n * 0.4f;
    float period = t * s;
""" + TAIL),
    ("0.4f * n * scale", H + DECLS + HEAD + """
    float period = 0.4f * (float)n * r->scale;
""" + TAIL),
    ("inlined helper for the product", H + DECLS + """
static float Period(int n)
{
    return (float)n * 0.4f;
}
""" + HEAD + """
    float period = Period(n) * r->scale;
""" + TAIL),
]
