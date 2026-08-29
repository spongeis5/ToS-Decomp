// sub_8259C778 -- read n bits (n <= 16) out of the same bit stream as
// sub_8259C6F0, and advance. 104 B, 5 callers.
//
// This is the 16-bit sibling of `a4_bitread24.cpp` and sits IMMEDIATELY after
// it -- 8259C6F0 + 132 = 8259C778 -- so the two are the same translation unit
// and the same `/O2`. Everything that decided that one decides this one:
//
//   * `260(r3)` is loaded THREE times, once per store to the stream, and the
//     reloads are the aliasing MSVC could not remove -- so the chain is
//     spelled out rather than named.
//   * `rotlwi r6,r11,0` is the CSE-copy fingerprint (MATCHED.md): the bit
//     position is spelled `d->stream->bit` at BOTH the shift and the update,
//     which forces the copy out of r11 before `lbz r11,1(r7)` overwrites it.
//   * `stw r5,18612(r8)` sits AFTER both `lbz`s, so the write-back of the bit
//     position is written after the bytes are read.
//   * `srawi r10,r3,3` is a SIGNED shift, so the bit position is an `int`;
//     `clrlwi r5,r6,29` beside it is the plain `& 7`.
//
// The two differences from the 24-bit form are the whole of it: there is NO
// `n == 0` guard (the body is entered unconditionally), only two bytes are
// taken, the mask is `clrlwi r5,r7,16` = 16 bits, and `subfic r8,r4,16` is
// `16 - n`.

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

u32 DecoderReadBits16(Decoder* d, int n)
{
    const u8* p = d->stream->ptr;

    u32 v = ((u32)p[0] << 8) | p[1];

    v = (v << d->stream->bit) & 0x0000FFFFu;
    d->stream->bit = d->stream->bit + n;

    d->stream->ptr += d->stream->bit >> 3;
    d->stream->bit &= 7;
    return v >> (16 - n);
}
