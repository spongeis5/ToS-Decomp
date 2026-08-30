// sub_82166B50 -- add a byte delta to a table's item pointer and then to
// eight pointer fields inside every 296-byte record it holds. 208 bytes,
// 2 callers. A pointer-fixup pass of the kind that runs once after a block
// of records is loaded at an address other than the one it was built for.
//
//      lwz  r10,4(r3)        the item pointer
//      addi r11,r3,4         ITS ADDRESS -- computed and never read
//      add  r11,r10,r4
//      stw  r11,4(r3)
//
// The dead `addi` is MATCHED.md's signature of an inlined helper taking the
// member's address, and the address-of lever is also what keeps the later
// `lwz r10,0(r3)` (the count) from being hoisted above the store.
//
// The record stride is `mulli r11,r10,296`, so the element size is 296 and
// the eight fields sit at 48, 52, 60, 64, 72, 76, 284 and 288 -- read off
// the biased base `addi r11,r9,64` and the displacements -16, -12, -4, 0,
// 8, 12, 220, 224. Four of them are unconditional and four are guarded by a
// null test, and the guarded four each carry an extra `rotlwi r10,r10,0`,
// which per MATCHED.md is a common subexpression being COPIED -- the source
// reads the field again in the assignment rather than using `+=`.
//
// The loop is `while (p != end)`: `cmplw`/`beqlr` on entry and `bne+` at the
// bottom, an equality test in both places, so it is a rotated `while` over a
// half-open range and not a counted loop.
//
// NEAR MISS: 184 bytes against 208, 7 of 46 words. The HEADER is right --
// the dead `addi r11,r3,4` appears only with the inlined `Fix(&t->items, d)`
// helper below, and writing `t->items = (Rec*)((char*)t->items + delta);`
// inline instead loses it AND lets the `lwz r10,0(r3)` count load hoist
// above the store (176 bytes, 1 of 44). Two things in the LOOP are still
// missing, six words between them:
//
//   * the BIASED SECOND INDUCTION VARIABLE. Retail carries r9 (stride 296,
//     used only for the loop test) and r11 = r9 + 64 (used for every
//     access, at displacements -16, -12, -4, 0, 8, 12, 220, 224). We
//     generate one pointer and address everything from it at 48..288. All
//     the displacements fit in 16 bits either way, so the bias buys retail
//     nothing and is not forced by the offsets -- it is an
//     induction-variable choice, and no spelling tried has reproduced it.
//   * the four `rotlwi r10,r10,0` COPIES, one in each null-guarded field.
//     Per MATCHED.md that self-move is a common subexpression being copied,
//     so the source repeated the read; but `if (*p != 0) *p = *p + d;`
//     inside the helper already spells it twice and MSVC common-
//     subexpressions it away. The four unguarded fields have no copy, so
//     whatever produces it is specific to the guarded shape.
//
// Note which way the helper cuts: it is REQUIRED for the header (the dead
// address computation) and does not by itself produce either loop feature.

#include "types.h"

struct Rec
{
    /* 0x000 */ u8    unk0000[0x30];
    /* 0x030 */ char* a;
    /* 0x034 */ char* b;
    /* 0x038 */ u8    unk0038[0x04];
    /* 0x03C */ char* c;
    /* 0x040 */ char* d;
    /* 0x044 */ u8    unk0044[0x04];
    /* 0x048 */ char* e;
    /* 0x04C */ char* f;
    /* 0x050 */ u8    unk0050[0xCC];
    /* 0x11C */ char* g;
    /* 0x120 */ char* h;
    /* 0x124 */ u8    unk0124[0x04];
};
ASSERT_OFFSET(Rec, a, 0x030);
ASSERT_OFFSET(Rec, b, 0x034);
ASSERT_OFFSET(Rec, c, 0x03C);
ASSERT_OFFSET(Rec, d, 0x040);
ASSERT_OFFSET(Rec, e, 0x048);
ASSERT_OFFSET(Rec, f, 0x04C);
ASSERT_OFFSET(Rec, g, 0x11C);
ASSERT_OFFSET(Rec, h, 0x120);
ASSERT_SIZE(Rec, 296);

struct RecTable
{
    /* 0x00 */ s32  count;
    /* 0x04 */ Rec* items;
};
ASSERT_OFFSET(RecTable, items, 0x04);

static void Fix(char** p, s32 d)
{
    *p = *p + d;
}

static void FixOpt(char** p, s32 d)
{
    if (*p != 0)
        *p = *p + d;
}

void RelocateRecs(RecTable* t, s32 delta)
{
    Fix((char**)&t->items, delta);

    Rec* p = t->items;
    Rec* end = p + t->count;
    while (p != end)
    {
        Fix(&p->a, delta);
        FixOpt(&p->b, delta);
        Fix(&p->c, delta);
        FixOpt(&p->d, delta);
        Fix(&p->e, delta);
        FixOpt(&p->f, delta);
        Fix(&p->g, delta);
        FixOpt(&p->h, delta);
        p++;
    }
}
