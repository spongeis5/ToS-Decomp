// sub_8216E778 -- add a delta to a base cursor and then to every embedded
// pointer of a two-level table hanging off it. 116 B, 5 callers.
//
//   lwz  r10,12(r3)
//   addi r11,r3,12        <- computed and then IMMEDIATELY overwritten
//   add  r11,r10,r4
//   stw  r11,12(r3)
//   rotlwi r10,r11,0
//
// The dead `addi r11,r3,12` is an address whose result is never read, and the
// inner update has the matching `rotlwi r11,r11,0` copy with `lbz r9,0(r10)`
// BELOW its store rather than hoisted above it. Both are MATCHED.md's
// address-of lever, and both are needed:
//
//   plain `t->nodes = delta + t->nodes` and `p->kids = delta + p->kids`
//                                       104 B, 1 of 26 -- BOTH copies absent
//   a static helper `Bump(u32* p, int d)` at all three sites
//                                       116 B, 28 of 29
//   `u32* s = &t->nodes; *s = delta + *s;` at all three sites
//                                       116 B, 29 of 29
//   the same with `u32&` references     116 B, 29 of 29
//   the helper on the table field only  112 B, 10 of 28
//
// Nine shapes measured. The address has to be taken at EVERY site, not just
// the one whose `addi` survives: with the helper used only on `t->nodes` the
// inner copy is still missing and the whole tail shifts. The helper and the
// local differ by one word, so what MSVC keeps is the local pointer itself,
// not the call boundary.
//
// Both loops are `cmplw` + `beq`-out-of-line at the top and `cmplw` + `bne+`
// at the bottom, which is MSVC's rotation of a `while (p != end)`. The ends
// are built as `slwi 3` + `add`, so both element sizes are 8: the outer count
// is a `u16` at +8 of the object (`lhz`) and the inner count a `u8` at +0 of
// the element (`lbz`).
//
// The delta is added to the pointers as a plain integer, so the fields hold
// ADDRESSES as words rather than typed pointers -- this is a relocation pass
// over a block that was loaded somewhere other than where it was built.

#include "types.h"

struct Node
{
    /* 0x00 */ u8  n;
    /* 0x01 */ u8  unk0001[3];
    /* 0x04 */ u32 kids;
};

ASSERT_SIZE(Node, 8);
ASSERT_OFFSET(Node, kids, 4);

struct Table
{
    /* 0x00 */ u8  unk0000[8];
    /* 0x08 */ u16 count;
    /* 0x0A */ u8  unk000A[2];
    /* 0x0C */ u32 nodes;
};

ASSERT_OFFSET(Table, count, 8);
ASSERT_OFFSET(Table, nodes, 12);

void RelocateChains(Table* t, int delta)
{
    u32* s = &t->nodes;
    *s = delta + *s;

    Node* p   = (Node*)t->nodes;
    Node* end = p + t->count;

    while (p != end)
    {
        u32* a = &p->kids;
        *a = delta + *a;

        Node* q    = (Node*)p->kids;
        Node* qend = q + p->n;

        while (q != qend)
        {
            u32* b = &q->kids;
            *b = delta + *b;
            q = q + 1;
        }

        p = p + 1;
    }
}
