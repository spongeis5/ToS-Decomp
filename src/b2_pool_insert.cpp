#include "types.h"

// sub_825A3528 -- claim the first free slot of a 552-byte-stride pool, link it
// into a doubly linked list before a given node, initialise it, and optionally
// hand it back. Returns 0, or 33 when the pool is full. 228 B, 6 callers.
//
//   lwz r8,1044(r3) ; li r10,0 ; mr r11,r10 ; cmpwi cr6,r8,0 ; ble- full
//   lwz r7,1048(r3) ; addi r9,r7,16
//   L: lbz r3,0(r9) ; cmplwi cr6,r3,0 ; beq- found
//      addi r11,r11,1 ; addi r9,r9,552 ; cmpw cr6,r11,r8 ; blt+ L
//   full: li r3,33 ; blr
//   found: mulli r11,r11,552 ; add r11,r11,r7
//      ... constants materialised, then the stores ...
//
// Layout facts, all forced by the listing:
//   * stride 552 (the `mulli` and the `addi r9,r9,552`), in-use byte at +16;
//   * count at 0x414 (`cmpwi` -- SIGNED, so s32), items at 0x418;
//   * the link fixup is the textbook insert-before at +0/+4.
//
// `lwz r9,4(r11)` RELOADS e->next one instruction after `stw r11,4(r9)`
// wrote through the previous node -- the store could alias e itself, so the
// compiler cannot forward it. That reload is not a spelling choice.
//
// The `li r10,0` hoisted to the very first block is the zero shared by
// `i = 0` and by every zero store below; `mr r11,r10` is the copy that makes.
//
// r5 is a parameter that is never read -- it is overwritten with the constant
// 128 -- but r6 IS read, so the function takes four arguments and the third
// is unused. Dropping it would put the out pointer in r5.
//
// NEAR MISS, and the whole of it is ONE EXTRA LIVE VALUE. Ours is 240 bytes
// against 228 -- three words: `std r31`, and an `ld r31` on each of the two
// return paths -- and because the spill displaces everything, 0 of 57 words
// line up. The instruction SEQUENCE is otherwise identical, slot for slot.
//
// The initialisation block needs six constants live at once (0, 64,
// 0x00400000, 128, 0x00800000, 1024) plus e, out, `at` and one scratch. That
// is ten values against nine volatile registers. The retail build fits by
// materialising its SIXTH constant after `stw r4,4(r11)` has killed the `at`
// parameter, into the register `at` vacated:
//
//   li r5,128 ; stb r9,16(r11) ; li r3,1024 ; ... ; stw r4,4(r11)
//   lis r4,128                          <- 0x00800000, into the dead r4
//
// We materialise ours four slots earlier, while r4 is still live, so it goes
// to r31 and pays a frame.
//
// What was measured:
//  * store order. Written ASCENDING, the constants come out
//    0x400000, 64, 0x800000, 128 -- the first two in the wrong registers.
//    Written with 472 before 468 and 496 before 492, as the image emits them,
//    the first FOUR constants land in the target's own registers
//    (r10, r8, r7, r5). That is why the two pairs are written descending here.
//  * which constant is fifth follows source DEF ORDER: moving
//    `f214 = 1024` to just before `f1EC` puts `li r3,1024` in the target's
//    slot exactly -- but then the 532 store is emitted there too, in the
//    middle of the block, and the image has it last. MSVC does not sink it.
//    So the retail 5th/6th choice is NOT def order; it is an allocator
//    decision the source cannot reach from either ordering.
//  * member function instead of free: 240 bytes, same spill.
//  * tools/flagsweep.py, 72 combinations including /Ou prescheduling: every
//    one spills. Best 0 of 57 words, 236 B at /O2 /Os.

struct Entry552
{
    /* 0x000 */ Entry552* prev;
    /* 0x004 */ Entry552* next;
    /* 0x008 */ char      unk0008[0x08];
    /* 0x010 */ u8        used;
    /* 0x011 */ char      unk0011[0x192];
    /* 0x1A3 */ u8        f1A3;
    /* 0x1A4 */ char      unk01A4[0x28];
    /* 0x1CC */ u32       f1CC;
    /* 0x1D0 */ u32       f1D0;
    /* 0x1D4 */ u32       f1D4;
    /* 0x1D8 */ u32       f1D8;
    /* 0x1DC */ u32       f1DC;
    /* 0x1E0 */ u8        f1E0;
    /* 0x1E1 */ char      unk01E1[0x03];
    /* 0x1E4 */ u32       f1E4;
    /* 0x1E8 */ u32       f1E8;
    /* 0x1EC */ u32       f1EC;
    /* 0x1F0 */ u32       f1F0;
    /* 0x1F4 */ u32       f1F4;
    /* 0x1F8 */ u8        f1F8;
    /* 0x1F9 */ char      unk01F9[0x03];
    /* 0x1FC */ u32       f1FC;
    /* 0x200 */ u32       f200;
    /* 0x204 */ u32       f204;
    /* 0x208 */ u32       f208;
    /* 0x20C */ u32       f20C;
    /* 0x210 */ u8        f210;
    /* 0x211 */ char      unk0211[0x03];
    /* 0x214 */ u32       f214;
    /* 0x218 */ char      unk0218[0x10];
};
ASSERT_OFFSET(Entry552, next, 0x004);
ASSERT_OFFSET(Entry552, used, 0x010);
ASSERT_OFFSET(Entry552, f1A3, 0x1A3);
ASSERT_OFFSET(Entry552, f1CC, 0x1CC);
ASSERT_OFFSET(Entry552, f1E0, 0x1E0);
ASSERT_OFFSET(Entry552, f1F8, 0x1F8);
ASSERT_OFFSET(Entry552, f214, 0x214);
ASSERT_SIZE(Entry552, 552);

struct Pool552
{
    /* 0x000 */ char       unk0000[0x414];
    /* 0x414 */ s32        count;
    /* 0x418 */ Entry552*  items;
};
ASSERT_OFFSET(Pool552, count, 0x414);
ASSERT_OFFSET(Pool552, items, 0x418);

s32 PoolInsert(Pool552* p, Entry552* at, s32 unused, Entry552** out)
{
    s32 n = p->count;

    for (s32 i = 0; i < n; i++)
    {
        if (p->items[i].used == 0)
        {
            Entry552* e = &p->items[i];

            e->used = 1;
            e->prev = at->prev;
            e->next = at;
            e->prev->next = e;
            e->next->prev = e;

            e->f1A3 = 0;
            e->f1CC = 0;
            e->f1D0 = 0;
            e->f1D8 = 64;
            e->f1D4 = 0x00400000;
            e->f1DC = 0;
            e->f1E0 = 0;
            e->f1E4 = 0;
            e->f1E8 = 0;
            e->f1F0 = 128;
            e->f1EC = 0x00800000;
            e->f1F4 = 0;
            e->f1F8 = 0;
            e->f1FC = 0;
            e->f200 = 0;
            e->f204 = 0;
            e->f208 = 0;
            e->f20C = 0;
            e->f210 = 0;
            e->f214 = 1024;

            if (out != 0)
                *out = e;

            return 0;
        }
    }

    return 33;
}
