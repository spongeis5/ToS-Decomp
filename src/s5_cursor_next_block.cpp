#include "types.h"

// sub_82780C20 -- move a cursor on to the next block when the current one is
// used up. 88 B, 7 callers.
//
//   lwz   r10,8(r3)      c->mark
//   lhz   r11,20(r3)     c->off        (u16, zero-extended)
//   lwz   r9,4(r3)       c->pos
//   subf  r11,r11,r10    b   = mark - off        <- the block base
//   subf  r10,r11,r9     pos - b
//   addi  r10,r10,-8     ... - 8
//   lwz   r9,4(r11)      b->size
//   cmpw  cr6,r10,r9     SIGNED
//   bltlr cr6            still room -- nothing to do
//   lwz   r11,0(r11)     n = b->next
//   cmplwi cr6,r11,0     \
//   addi  r10,r11,8       |  the +8 upcast idiom: null stays null
//   bne-  cr6,go          |
//   li    r10,0          /
// go:
//   stw   r10,4(r3)      c->pos  = d
//   cmplwi cr6,r11,0
//   beqlr cr6            no next block: pos is now null and that is all
//   rotlwi r10,r10,0     <- the CSE copy of d
//   subf  r11,r11,r10    d - (char*)n
//   stw   r10,8(r3)      c->mark = d
//   sth   r11,20(r3)     c->off  = (u16)(d - (char*)n)
//   blr
//
// `mark - off` is recomputed on every call, so `off` is exactly the distance
// from the block base to `mark`, and the tail keeps that invariant by storing
// the same distance it just used.
//
// The subtraction the compiler will NOT fold is the point of the function's
// shape: `d - (char*)n` is 8 by inspection, but `d` came out of a
// null-preserving adjust, so on the null edge it would be 0 and MSVC cannot
// constant-fold across the merge. That is also why the second `cmplwi` tests
// `n` again rather than `d`.
//
// NOT MATCHED: 12 of 21 words at /O2 /Os, 84 bytes against 88.
//
// The one missing word is `rotlwi r10,r10,0` at 82780C64 -- a move to ITSELF,
// which MATCHED.md records as the fingerprint of a common subexpression being
// COPIED. From the `next` load onward r10 and r11 are transposed as well: the
// target REUSES the register that held `b` for `n`, and every spelling tried
// allocates a fresh one.
//
// One word came from BRANCH POLARITY on the inner guard: testing `d == 0`
// rather than `n == 0` after the conditional adjust is 12 of 21 where testing
// `n` is 11. They are the same condition -- `d` is null exactly when `n` is --
// so this is a spelling choice and not a semantic one, and it is worth a word.
//
// THIRTEEN SHAPES MEASURED, all 84 bytes and none past 12 of 21:
//
//   * reusing `b` for the next pointer instead of declaring `n` -- which was
//     the direct reading of the register reuse -- 11 of 21, byte-identical
//     to the baseline;
//   * the same with `d` unnamed, with `d` copied into a second local, and
//     with the swap written through the other side;
//   * the block header as a real base-class upcast rather than a cast;
//   * the offset named before the mark store;
//   * reading `c->mark` back after storing it, and reading `c->pos` back --
//     the memory-round-trip forwarding that produced `mr r10,r7` in the
//     arena -- both byte-identical;
//   * the difference computed as `(u32)d - (u32)b`, and through an
//     `unsigned` local;
//   * the block base recomputed in the tail through a longer chain.
//
// So the copy is not reachable from how the difference or the pointer is
// spelled. What it would take is for `d` to be live in two places at once,
// and nothing in a function this short creates that pressure.

struct CurBlk
{
    /* 0x00 */ CurBlk* next;
    /* 0x04 */ s32     size;
};
ASSERT_OFFSET(CurBlk, size, 0x04);

struct BlkCursor
{
    /* 0x00 */ u32   unk0000;
    /* 0x04 */ char* pos;
    /* 0x08 */ char* mark;
    /* 0x0C */ char  unk000C[0x08];
    /* 0x14 */ u16   off;
};
ASSERT_OFFSET(BlkCursor, pos, 0x04);
ASSERT_OFFSET(BlkCursor, mark, 0x08);
ASSERT_OFFSET(BlkCursor, off, 0x14);

void CursorNextBlock(BlkCursor* c)
{
    CurBlk* b = (CurBlk*)(c->mark - c->off);

    if (c->pos - (char*)b - 8 < b->size)
        return;

    CurBlk* n = b->next;
    char*   d = n ? (char*)n + 8 : 0;

    c->pos = d;
    if (d == 0)
        return;

    c->mark = d;
    c->off = (u16)(d - (char*)n);
}
