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
// The field at 272 is loaded TWICE with nothing stored between, and the two
// loads are in different blocks: two separate `if` statements, not one
// expression. The second block is reached both by falling through the first
// and by the `blt-`, which is what pins the shape --
//
//      if (pos >= end && (flags & 8) == 0)  { *out = 0; return 0; }
//      if (flags & 0x100)                   { *out = 0; return 0; }
//
// with the `blt-` being the `&&` failing its first term and jumping straight
// to the second statement. The two `*out = 0` exits are then TAIL-MERGED into
// the single block C, and the outer guard's `*out = 1` shares block D with
// the fall-out.
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

    if (s != 0)
    {
        if (p->cursor->pos >= s->end && (p->flags & 8) == 0)
        {
            *out = 0;
            return 0;
        }

        if (p->flags & 0x100)
        {
            *out = 0;
            return 0;
        }
    }

    *out = 1;
    return 0;
}
