"""Round 5 for sub_82673EC8 -- the last untried axis: `> 0` in the GUARDS.

Rounds 1-4 varied the control flow (nine shapes), the destination type
(bool*, u8*, volatile bool*, bool&), the helper scope (three), the element
type of the table, and `> 0` versus `!= 0` in the TAIL, and every one lands
at 16 of 36 with the same two defects.  `tools/flagsweep.py` then compiled
all 72 flag combinations: 44 give 16 of 36 and 28 give 1 of 36, so the
optimisation level is exhausted too.

What was never tried is MATCHED.md's `x > 0` lever inside the two guards
themselves.  On sub_825FD7C0 that was worth five words precisely because it
FORCES the value to be materialised where `!= 0` folds -- and folding is
exactly what if-converts the second guard here.
"""

H = '#include "types.h"\n\n'

DECL = """
struct Matrix
{
    /* 0x00 */ char unk0000[52];
    /* 0x34 */ u32  bits[32];
};
ASSERT_OFFSET(Matrix, bits, 52);
"""

TAIL = "    *out = (m->bits[a & 31] & (1u << (b & 31))) != 0;\n"


def mk(mid, decls=""):
    return (H + DECL + decls +
            "\nvoid PairRelated(bool* out, const Matrix* m, u32 a, u32 b)\n{\n"
            "    if (((a ^ b) & 0xFFFF0000) == 0 && (a & 0xFFFF0000) != 0)\n"
            "    {\n" + mid + "    }\n" + TAIL + "}\n")


BODIES = [
    ("guards as !(x > 0)", mk("""        u32 x = ((b >> 5) ^ a) & 0x3E0;
        if (!(x > 0)) { *out = false; return; }
        u32 y = ((a >> 5) ^ b) & 0x3E0;
        if (!(y > 0)) { *out = false; return; }
        *out = true;
        return;
""")),
    ("positive path as x > 0 && y > 0", mk("""        u32 x = ((b >> 5) ^ a) & 0x3E0;
        u32 y = ((a >> 5) ^ b) & 0x3E0;
        if (x > 0 && y > 0) { *out = true; return; }
        *out = false;
        return;
""")),
    ("helper Any(v) { return v > 0; }", mk("""        if (!Any(((b >> 5) ^ a) & 0x3E0)) { *out = false; return; }
        if (!Any(((a >> 5) ^ b) & 0x3E0)) { *out = false; return; }
        *out = true;
        return;
""", "\nstatic bool Any(u32 v) { return v > 0; }\n")),
    ("guards as while", mk("""        while ((((b >> 5) ^ a) & 0x3E0) == 0) { *out = false; return; }
        while ((((a >> 5) ^ b) & 0x3E0) == 0) { *out = false; return; }
        *out = true;
        return;
""")),
    ("truthiness, no == 0", mk("""        if (!(((b >> 5) ^ a) & 0x3E0)) { *out = false; return; }
        if (!(((a >> 5) ^ b) & 0x3E0)) { *out = false; return; }
        *out = true;
        return;
""")),
    ("second guard as > 0 only", mk("""        if ((((b >> 5) ^ a) & 0x3E0) == 0) { *out = false; return; }
        u32 y = ((a >> 5) ^ b) & 0x3E0;
        if (!(y > 0)) { *out = false; return; }
        *out = true;
        return;
""")),
    ("s32 masked values, > 0", mk("""        s32 x = (s32)(((b >> 5) ^ a) & 0x3E0);
        if (!(x > 0)) { *out = false; return; }
        s32 y = (s32)(((a >> 5) ^ b) & 0x3E0);
        if (!(y > 0)) { *out = false; return; }
        *out = true;
        return;
""")),
    ("guards compare against a named zero", mk("""        u32 zero = 0;
        if ((((b >> 5) ^ a) & 0x3E0) == zero) { *out = false; return; }
        if ((((a >> 5) ^ b) & 0x3E0) == zero) { *out = false; return; }
        *out = true;
        return;
""")),
]
