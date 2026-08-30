#include "types.h"

// sub_82156E58 -- MT19937-shaped state shuffle: 227 rounds over the first
// block, 396 over the rest, then one final word. 220 B, 4 callers.
//
// Each round (r11 walking words, r11-908 pairing 99 words back):
//   w  = cur[1] (upper bit via rlwimi 31,0) | cur[0] (lower 31 bits)
//   lw = prev[-908/4]
//   out = (w >> 1) ^ lw ^ (0x9908B0DF if (w & 1) == 0 else 0)
// The conditional-constant is materialised branchlessly: clrlwi bit0,
// subfic 0, subfe (all-ones when bit clear), and with 0x9908B0DF.
//
// Head: li 227 rounds; g = 8298F604; 624 stored to 8298A6BC-20668?? no:
// stw r8,20668(r6) with r6 = 0x82930000 -> 829A06BC = the index store.

struct MtState
{
    /* 0x000 */ u32 words[624];
};

extern MtState g_mt_8298F604;

void MtShuffle()
{
    unsigned int* y = &g_mt_8298F604.words[1];
    int n = 227;
    while (n--)
    {
        unsigned int w = (y[0] & 0x80000000u) | (y[-1] & 0x7FFFFFFFu);
        unsigned int lw = y[98];
        unsigned int bits = w & 1u;
        unsigned int mask = 0u - bits;
        y[-1] = (w >> 1) ^ lw ^ (mask & 0x9908B0DFu);
        y += 1;
    }
    n = 396;
    while (n--)
    {
        unsigned int w = (y[0] & 0x80000000u) | (y[-1] & 0x7FFFFFFFu);
        unsigned int lw = y[-227];
        unsigned int bits = w & 1u;
        unsigned int mask = 0u - bits;
        y[-1] = (w >> 1) ^ lw ^ (mask & 0x9908B0DFu);
        y += 1;
    }
    unsigned int w = (g_mt_8298F604.words[0] & 0x80000000u)
                     | (y[-1] & 0x7FFFFFFFu);
    unsigned int bits = w & 1u;
    unsigned int mask = 0u - bits;
    y[-1] = (w >> 1) ^ g_mt_8298F604.words[397]
            ^ (mask & 0x9908B0DFu);
}

// NEAR-MISS. MT-shaped; rlwimi/rlwinm compositions not reproduced.
