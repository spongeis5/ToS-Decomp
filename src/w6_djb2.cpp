#include "types.h"

// sub_8276DCA8 -- combine a 65599 string hash with packed header fields.
// 276 B, 4 callers.
//
//   h = 0
//   if (f30 & 0x40) and str: the string's first word is a byte LENGTH;
//     h = 5381; for i = len..1: h = h * 65599 + p[i-1]  (mullw 0x1003F)
//   if (f30 & 0x0002) h ^= f20                       (zero-extended)
//   if (f30 & 0x0004) h ^= (s32)f22 << 8             (sign-extended lha)
//   if (f30 & 0x0008) h ^= (s32)f24 << 12
//   if (f30 & 0x0010) h ^= f26 << 16
//   if (f30 & 0x0020) h ^= f28 << 18
//   if (f30 & 0x0100) bit = (f30 & 0x8000) ? 1 : 0
//   mix = ((f30 rotl 24) & 0x60000000) | (f30 rotl 9) | bit
//   mix ^= (f30 rotr 1) & 0x00300000
//   r3 = mix ^ h

struct Hashed
{
    /* 0x10 */ char* str;
    /* 0x14 */ u16   f20;
    /* 0x16 */ s16   f22;
    /* 0x18 */ s16   f24;
    /* 0x1A */ u16   f26;
    /* 0x1C */ u16   f28;
    /* 0x1E */ u16   f30;
};

static u32 rotl(u32 v, int s)
{
    return (v << s) | (v >> (32 - s));
}

u32 HashCombine(Hashed* h)
{
    u32 acc = 0;
    if (h->f30 & 0x40)
    {
        if (h->str != 0)
        {
            u32 x = 5381;
            unsigned char* p = (unsigned char*)h->str;
            u32 len = *(u32*)p;
            u32 bytes = len * 4;
            if (bytes)
            {
                for (u32 i = bytes; i; --i)
                    x = x * 65599u + p[i - 1];
            }
            acc = x;
        }
    }
    if (h->f30 & 0x0002)
        acc ^= h->f20;
    if (h->f30 & 0x0004)
        acc ^= (u32)((s32)h->f22 << 8);
    if (h->f30 & 0x0008)
        acc ^= (u32)((s32)h->f24 << 12);
    if (h->f30 & 0x0010)
        acc ^= (u32)h->f26 << 16;
    if (h->f30 & 0x0020)
        acc ^= (u32)h->f28 << 18;
    u32 bit = 0;
    if (h->f30 & 0x0100)
        bit = (h->f30 & 0x8000u) ? 1u : 0u;
    u32 mix = ((rotl(h->f30, 24)) & 0x60000000u) | rotl(h->f30, 9) | bit;
    mix ^= rotl(h->f30, 31) & 0x00300000u;
    return mix ^ acc;
}

// NEAR-MISS. 3/69 -- tail rotl composition and loop walk wrong.
