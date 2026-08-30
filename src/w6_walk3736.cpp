#include "types.h"

// sub_82528530 -- walk a fixed chain, returning small status codes and an
// optional out-pointer. 84 B, 3 callers.
//
//   if (arg == 0)  return 37
//   p = o->f60;  if (!p) { *out = p; return 36; }
//   p = p->f24;  if (!p) { *out = 0; return 0; }
//   *out = p->f164; return 0
//
// The first guard is bne- AWAY to the interesting path (early-return
// polarity); the *out stores land after their li in every arm.

struct Walk1
{
    /* 0x18 */ char   unk0000[24];
    /* 0x18 */ Walk1* f24;
    /* 0x1C */ char   unk001C[136];
    /* 0xA4 */ void*  f164;
};

struct WalkRoot
{
    /* 0x3C */ char    unk0000[60];
    /* 0x3C */ Walk1*  f60;
};

ASSERT_OFFSET(Walk1, f24, 24);
ASSERT_OFFSET(Walk1, f164, 164);
ASSERT_OFFSET(WalkRoot, f60, 60);

int WalkStatus(WalkRoot* o, void** out)
{
    if (o == 0)
        return 37;
    Walk1* p = o->f60;
    if (p == 0)
    {
        *out = p;
        return 36;
    }
    p = p->f24;
    if (p == 0)
    {
        *out = 0;
        return 0;
    }
    *out = p->f164;
    return 0;
}

// NEAR-MISS. status-code walk; polarities right, block order differs.
