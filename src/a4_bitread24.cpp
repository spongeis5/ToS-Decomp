// sub_8259C6F0 -- read n bits (n <= 24) out of a bit stream reached through
// a field of the object, and advance. 132 B, 6 callers.
//
//   mr r11,r3 ; cmpwi cr6,r4,0 ; bne- 8259C704 ; li r3,0 ; blr
//
// The zero return is the FALL-THROUGH of the test, so it is written first:
// `if (n == 0) return 0;`.
//
//   lwz r9,260(r11) ; lwz r8,18616(r9) ; lwz r10,18612(r9)
//
// The pointer is read before the bit position. Three bytes are taken
// big-endian into a 24-bit value, shifted up by the current bit position,
// re-masked to 24 bits and shifted down by (24 - n):
//
//   rotlwi r8,r5,8 ; or r6,r8,r3 ; rlwinm r5,r6,8,0,23 ; or r3,r5,r10
//   slw r10,r3,r7 ; clrlwi r7,r10,8 ; srw r3,r7,r5      r5 = 24 - n
//
// `260(r11)` is loaded THREE times -- once for the read, once for the
// pointer advance and once for the bit wrap -- with a store to the stream in
// between each, so the source spells the chain out rather than naming it;
// the reloads are the aliasing MSVC could not remove.
//
// `srawi r9,r6,3` on the second read is a SIGNED shift, so the bit position
// is an `int`; `clrlwi r9,r10,29` is the plain `& 7` that follows it.
//
// TWO ORDERING FACTS decided this one, and each was worth several words:
//
//   * `stw r6,18612(r9)` sits AFTER all three `lbz`s. A store cannot move
//     above a load that might alias it, so the bit position is written back
//     AFTER the bytes are read, not before. With the store first the whole
//     schedule shifts and `d` stays in r3 -- 7 of 32 words.
//
//   * `rotlwi r7,r10,0` is the CSE-copy fingerprint (MATCHED.md), and it is
//     the whole reason the target is one instruction longer than the obvious
//     source. Naming `int bit = d->stream->bit;` and using it twice gives ONE
//     register and no copy, 128 bytes against 132. Spelling
//     `d->stream->bit` out at BOTH the shift and the update makes the two
//     uses a common subexpression that has to be copied out of r10 before
//     `lbz r10,2(r8)` overwrites it. 33 of 33.

#include "types.h"

struct BitStream
{
    /* 0x0000 */ u8        unk0000[0x48B4];
    /* 0x48B4 */ int       bit;
    /* 0x48B8 */ const u8* ptr;
};

ASSERT_OFFSET(BitStream, bit, 0x48B4);
ASSERT_OFFSET(BitStream, ptr, 0x48B8);

struct Decoder
{
    /* 0x0000 */ u8         unk0000[0x104];
    /* 0x0104 */ BitStream* stream;
};

ASSERT_OFFSET(Decoder, stream, 0x104);

u32 DecoderReadBits(Decoder* d, int n)
{
    if (n == 0)
        return 0;

    const u8* p = d->stream->ptr;

    u32 v = ((u32)p[0] << 8) | p[1];
    v = (v << 8) | p[2];

    v = (v << d->stream->bit) & 0x00FFFFFFu;
    d->stream->bit = d->stream->bit + n;

    d->stream->ptr += d->stream->bit >> 3;
    d->stream->bit &= 7;
    return v >> (24 - n);
}
