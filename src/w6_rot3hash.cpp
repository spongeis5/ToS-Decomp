#include "types.h"

// sub_8216B040 -- advance a rolling substitution window by one byte.
// 180 B, 4 callers.
//
// Head: n = side->f8 + t->i; shift = side->fC; end = (((n-1) >> shift) +
// (t->state >> shift)) & 0xFF.  While i != end: c = t->bytes[i]; i is
// masked to a byte after incrementing; then four linked byte writes over
// side->base, each indexed by rot3 of the previous value (rotlwi 3 on a
// byte = (c<<3)|(c>>5)), finishing with side->b24 = c.  On exit t->i and
// t->state are zeroed.
//
// t->side and side->base are re-read INSIDE the loop: the stores through
// them may alias t, so MSVC reloads both every iteration.

struct Side5
{
    /* 0x00 */ unsigned char* base;
    /* 0x08 */ s32            f8;
    /* 0x0C */ s32            fC;
    /* 0x18 */ unsigned char  b24;
};

struct Rot3
{
    /* 0x00 */ u32            state;
    /* 0x04 */ s32            i;
    /* 0x08 */ s32            f8;
    /* 0x0C */ Side5*         side;
    /* 0x10 */ unsigned char  bytes[232];
};

static unsigned char rot3(unsigned char c)
{
    return (unsigned char)((c << 3) | (c >> 5));
}

void Advance(Rot3* t)
{
    Side5* s = t->side;
    int n = s->f8 + t->i;
    int shift = s->fC;
    int end = (((n - 1) >> shift) + (t->state >> shift)) & 0xFF;
    while (t->i != end)
    {
        unsigned char c = t->bytes[t->i];
        t->i = (t->i + 1) & 0xFF;
        Side5* s2 = t->side;
        unsigned char* base = s2->base;
        unsigned char* x = base + rot3(c);
        x[1] = s2->b24;
        x[0] = base[rot3(s2->b24)];
        base[rot3(x[0])] = c;
        base[rot3(x[1])] = c;
        s2->b24 = c;
    }
    t->i = 0;
    t->state = 0;
}

// NEAR-MISS. 0/45 -- loop structure and aliasing reloads not yet matched; sizes differ 200 vs 180.
