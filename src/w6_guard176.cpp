#include "types.h"

// sub_8221D200 -- conditional state change plus a deferred check.
// 176 B, 4 callers.
//
//   if (x != 0 && f111 != 0 && f96 != 0)          -- two of the three
//     if (f109 != x) {                             -- guards are beqlr,
//       f109 = x;                                  -- conditional RETURNS
//       if (x != 0) {
//         f108 = 1; f100 = 1.0f;
//         g_word_829A7044->unk[x*12].f44 = 0;      -- sth to x*12
//       }
//   }
//   if (f109 == 0 && f108 != 0) {                  -- second guard pair
//     o = *this;
//     if (o->f68->f36 == 0.0f) Tail_8221D090(o);
//   }
//
// The tail 8221D090 is reached only by the final b; the .long 0 after it
// is this function's own padding.

struct GEntry
{
    /* 0x2C */ char unk002C[44];
};

struct GTable
{
    /* 0x7044 */ char unk0000[28740];
};

extern GTable* g_word_829A7044;

struct Obj68
{
    /* 0x24 */ float f36;
};

struct ObjA
{
    /* 0x44 */ Obj68* f68;
};

struct Guarded
{
    /* 0x00 */ ObjA*  self;
    /* 0x04 */ char   unk0004[92];
    /* 0x60 */ s32    f96;
    /* 0x64 */ float  f100;
    /* 0x68 */ char   unk0068[4];
    /* 0x6C */ u8     f108;
    /* 0x6D */ u8     f109;
    /* 0x6E */ u8     f110;
    /* 0x6F */ u8     f111;
};

ASSERT_OFFSET(Guarded, self, 0);
ASSERT_OFFSET(Guarded, f100, 100);
ASSERT_OFFSET(Guarded, f108, 108);
ASSERT_OFFSET(Guarded, f109, 109);
ASSERT_OFFSET(Guarded, f111, 111);

void Tail_8221D090(ObjA*);

void UpdateGuarded(Guarded* g, unsigned char x)
{
    unsigned char xv = (unsigned char)(x & 0xFF);
    if (xv != 0 && g->f111 != 0 && g->f96 != 0)
    {
        if (g->f109 != xv)
        {
            g->f109 = xv;
            if (xv != 0)
            {
                g->f108 = 1;
                g->f100 = 1.0f;
                *(short*)((char*)g_word_829A7044 + (size_t)xv * 12 + 44) = 0;
            }
        }
    }
    if (g->f109 == 0 && g->f108 != 0)
    {
        ObjA* o = g->self;
        if (o->f68->f36 == 0.0f)
            Tail_8221D090(o);
    }
}

// NEAR-MISS. three-guard beqlr form + deferred check; schedule differs.
