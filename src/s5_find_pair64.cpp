#include "types.h"

// sub_8215E6D0 -- linear search of a 16-byte-stride array for a key, writing
// the 8-byte payload at +8 through an out pointer, or zero when not found.
// 84 B, 7 callers.
//
//   lwz   r9,124(r4)     n = t->count      (0x7C, SIGNED -- cmpwi/ble-)
//   li    r11,0          i = 0
//   cmpwi cr6,r9,0
//   ble-  cr6,none
//   lwz   r8,120(r4)     t->items          (0x78, hoisted out of the loop)
//   mr    r10,r8         p = items
// L:lwz   r7,0(r10)      p->key
//   cmplw cr6,r7,r5      UNSIGNED key compare
//   beq-  cr6,hit
//   addi  r11,r11,1
//   addi  r10,r10,16     stride 16
//   cmpw  cr6,r11,r9     SIGNED loop test
//   blt+  cr6,L
// none:
//   li    r11,0
//   std   r11,0(r3)      a 64-BIT store of zero
//   blr
// hit:
//   rlwinm r11,r11,4,0,27    i * 16   -- the address is REBUILT from the
//   add    r11,r11,r8        index, because the hit block is out of line
//   ld     r10,8(r11)
//   std    r10,0(r3)
//   blr
//
// r3 is an OUT POINTER, not the container: it is never dereferenced for
// reading and is written with `std` on both exits. The container is r4 and
// the key is r5.
//
// ld/std, not two lwz/stw, so the payload is a single 64-bit object -- and
// `li r11,0 ; std r11,0(r3)` is the whole 64 bits set to zero, which a pair
// of 32-bit stores would never merge into.
//
// The loop carries BOTH an index and an induction pointer (r11 and r10) and
// the hit path uses neither of them directly: it recomputes items + i*16.
// That is the `t->items[i]` subscript written out at the hit rather than the
// walking pointer being reused -- the same shape as f_hash_probe.

struct PairEntry
{
    /* 0x00 */ u32 key;
    /* 0x04 */ u32 unk0004;
    /* 0x08 */ u64 value;
};
ASSERT_OFFSET(PairEntry, value, 0x08);
ASSERT_SIZE(PairEntry, 16);

struct PairTable
{
    /* 0x00 */ char       unk0000[0x78];
    /* 0x78 */ PairEntry* items;
    /* 0x7C */ s32        count;
};
ASSERT_OFFSET(PairTable, items, 0x78);
ASSERT_OFFSET(PairTable, count, 0x7C);

void FindPairValue(u64* out, PairTable* t, u32 key)
{
    for (s32 i = 0; i < t->count; ++i)
    {
        if (t->items[i].key == key)
        {
            *out = t->items[i].value;
            return;
        }
    }

    *out = 0;
}
