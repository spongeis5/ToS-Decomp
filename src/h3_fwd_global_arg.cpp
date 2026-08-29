#include "types.h"

// sub_821838E8 -- forward to a call on a global, argument shifted to r4.
// 16 B, 4 callers.
//
//      lis     r11,-32109
//      mr      r4,r3
//      addi    r3,r11,22180       ; = 829356A4
//      b       0x82183760
//
// Same shape as src/fwd_global.cpp (sub_821A4FA0) with a different global
// and a different callee. 3 of 4 words are relocated -- the `lis`/`addi`
// pair that forms 829356A4 and the tail branch -- so `mr r4,r3` is the only
// word actually compared.

struct GlobalRegistry;
extern GlobalRegistry g_registry_829356A4;

struct RegistryItem;

int RegistryAdd(GlobalRegistry* r, RegistryItem* item);

int AddToRegistry(RegistryItem* item)
{
    return RegistryAdd(&g_registry_829356A4, item);
}
