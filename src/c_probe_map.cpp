// sub_826376E0 -- linear-probe hash map keyed on a 32-bit id, Knuth's
// 0x9E3779B1 multiplier. 100 B, 42 callers.
//
//   lis    r10,-25033
//   lwz    r8,8(r3)          mask  = m->mask
//   rlwinm r11,r4,28,4,31    key >> 4
//   lwz    r7,0(r3)          table = m->entries
//   ori    r9,r10,31153      0x9E3779B1  (lis+ori, so an UNSIGNED constant)
//   mullw  r6,r11,r9
//   and    r11,r6,r8         i = hash & mask
//   rlwinm r10,r11,3,0,28    i * 8       -> the entry is 8 bytes
//   add    r9,r10,r7         &table[i]
//   lwzx   r10,r10,r7        table[i].key
//   cmpwi  cr6,r10,-1
//   beq-   cr6,notfound
// L:cmplw  cr6,r10,r4
//   beq-   cr6,found
//   addi   r11,r11,1
//   and    r11,r11,r8        i = (i + 1) & mask
//   rlwinm r10,r11,3,0,28
//   add    r9,r10,r7
//   lwzx   r10,r10,r7
//   cmpwi  cr6,r10,-1
//   bne+   cr6,L
// notfound: mr r3,r5 ; blr
// found:    lwz r3,4(r9) ; blr
//
// The loop is rotated: the first load and empty test are peeled ahead of the
// body, and the bottom of the body repeats them and branches back to L, which
// is INSIDE the peeled copy. That is a `while (slot is occupied)`, not a
// do/while -- a do/while would have entered at the top of the load.
//
// Two different compares on the same loaded word say what the types are:
// `cmpwi ...,-1` is signed against the empty marker, `cmplw` is unsigned
// against the key, so the key parameter is unsigned and the stored key is
// compared against -1 as a signed sentinel.

#include "types.h"

struct ProbeEntry
{
    int key;
    int value;
};
ASSERT_SIZE(ProbeEntry, 8);

struct ProbeMap
{
    ProbeEntry* entries;
    u32         unk0004;
    u32         mask;
};
ASSERT_OFFSET(ProbeMap, mask, 0x08);

int ProbeMapFind(ProbeMap* m, u32 key, int missing)
{
    u32         mask = m->mask;
    ProbeEntry* tbl = m->entries;
    u32         i = ((key >> 4) * 0x9E3779B1u) & mask;

    while (tbl[i].key != -1)
    {
        if (tbl[i].key == key)
            return tbl[i].value;
        i = (i + 1) & mask;
    }

    return missing;
}
