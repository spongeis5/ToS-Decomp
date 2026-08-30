#include "types.h"

// sub_82237F90 -- stash two floats on the object, then tail-call with four
// float arguments, three of them the same literal. 52 B, 3 callers.
//
//      mr      r11,r3           this kept across the arg setup
//      lwz     r3,56(r3)        arg 1 = this->f56
//      lfs     f3,11584(r9)     1.0                  82002D40
//      lfs     f0,11684(r10)    0.0                  82002DA4
//      fmr     f2,f3
//      stfs    f3,396(r11)      f396 = 1.0f          <- emitted first
//      fmr     f1,f3
//      lfs     f4,11972(r8)     -1.0                 82002EC4
//      stfs    f0,392(r11)      f392 = 0.0f
//      b       821E8060
//
// One literal 1.0f spelled three times materialises once and fmr's into
// f1/f2; f396 is stored before f392 because store order is source order.

struct WithFloats
{
    /* 0x38 */ char  unk0000[56];
    /* 0x38 */ void* target;
    /* 0x3C */ char  unk003C[332];
    /* 0x188 */ float f392;
    /* 0x18C */ float f396;
};

ASSERT_OFFSET(WithFloats, target, 56);
ASSERT_OFFSET(WithFloats, f392, 392);
ASSERT_OFFSET(WithFloats, f396, 396);

void Tail_821E8060(void* a, float, float, float, float);

void SetAndCall(WithFloats* s)
{
    const float one   = 1.0f;
    const float zero  = 0.0f;
    const float minus = -1.0f;
    s->f396 = one;
    s->f392 = zero;
    Tail_821E8060(s->target, one, one, one, minus);
}

// NO HOME: near-miss in a way neither list file can hold. Under match.py's
// standard this MATCHES -- but four of its words are relocated, and inside
// them the constant base registers are swapped (want lis r9/lfs f3 via r9;
// got lis r10/lfs f4 via r10, 0.0 via the other). build.py RESOLVES
// relocations and compares whole words, so those register fields surface
// and the build fails; the manifest cannot hold the row. The attempts list
// cannot either: its "no near-miss secretly matches" check re-scores with
// match.py's standard, reads MATCH, and demands promotion. Two owners, two
// standards, one row -- so the row lives nowhere and the account lives
// here. Named-constants, reversed store order and argument-reorder spellings
// were tried; the third float argument keeps materialising as f4.
