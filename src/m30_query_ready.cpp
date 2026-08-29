// sub_825914B8 -- answer a yes/no question through a byte out-parameter and
// return a zero status either way. 96 bytes, 4 callers.
//
//      lwz     r11,376(r3) ; cmplwi ; beq- cr6,<D: out = 1>
//      lwz     r10,392(r3)
//      lwz     r9,268(r11)
//      lwz     r8,36(r10)
//      cmplw   cr6,r8,r9 ; blt- cr6,<second test>
//      lwz     r11,272(r3) ; rlwinm r10,r11,0,28,28 ; beq- cr6,<C: out = 0>
//      lwz     r11,272(r3) ; rlwinm r10,r11,0,23,23 ; beq- cr6,<D: out = 1>
//  C:  li r11,0 ; li r3,0 ; stb r11,0(r4) ; blr
//  D:  li r11,1 ; li r3,0 ; stb r11,0(r4) ; blr
//
// Two `rlwinm` with NO rotate -- 0,28,28 is bit 0x8 and 0,23,23 is bit 0x100 --
// which per the single-bit rule is the mask tested IN PLACE, i.e. an inline
// `flags & K` and not a bool-returning accessor (that would rotate the bit
// down to 31).
//
// ONE `if` with a short-circuit condition, not two statements. Written as two
// separate `if (...) { *out = 0; return 0; }` the first body is planted INLINE
// as the fall-through of its own test and the polarity of the bit-3 test
// inverts to `bne-`: 9 of 24, and four bytes too long. That is the
// sub_8219FCD8 lever seen from the outer side -- a sequence of `if`s gives
// each guard a private exit, while one expression makes every true term share
// the same forward branch.
//
// The condition reads straight off the branch targets:
//
//      A && ((B && C) || D)
//
//   A  s != 0                 !A -> the *out = 1 block          beq- 82591508
//   B  pos >= end             !B -> skip to the D term          blt- 825914E8
//   C  (flags & 8) == 0        C -> the *out = 0 block          beq- 825914F8
//   D  flags & 0x100          !D -> the *out = 1 block          beq- 82591508
//
// The two exits are each a full `li ; li r3,0 ; stb ; blr`, which is the
// duplication an `if` body ending in `return` plus a fall-through return
// gives -- not a shared store fed by a merged value.
//
// The field at 272 is loaded TWICE with nothing stored between, because the
// two terms sit in different blocks.
//
// `cmplw` (not `cmpw`) on both halves of the comparison: unsigned.
//
// Nothing is relocated: 24 of 24 words are compared.

#include "types.h"

struct Stream
{
    /* 0x000 */ u8  unk0000[0x10C];
    /* 0x10C */ u32 end;
};

ASSERT_OFFSET(Stream, end, 0x10C);

struct Cursor
{
    /* 0x00 */ u8  unk0000[0x24];
    /* 0x24 */ u32 pos;
};

ASSERT_OFFSET(Cursor, pos, 0x24);

struct Player
{
    /* 0x000 */ u8      unk0000[0x110];
    /* 0x110 */ u32     flags;
    /* 0x114 */ u8      unk0114[0x64];
    /* 0x178 */ Stream* stream;
    /* 0x17C */ u8      unk017C[0x0C];
    /* 0x188 */ Cursor* cursor;
};

ASSERT_OFFSET(Player, flags, 0x110);
ASSERT_OFFSET(Player, stream, 0x178);
ASSERT_OFFSET(Player, cursor, 0x188);

int QueryFinished(Player* p, u8* out)
{
    Stream* s = p->stream;

    if (s != 0 && ((p->cursor->pos >= s->end && (p->flags & 8) == 0)
                   || (p->flags & 0x100)))
    {
        *out = 0;
        return 0;
    }

    *out = 1;
    return 0;
}
