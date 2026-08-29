// sub_825BFFF0 -- read up to 32 bits out of a byte-oriented bit stream,
// most-significant byte first, and advance the cursor. 236 B, 15 callers.
//
// The mask table it indexes is at 82062890 and is (1u << n) - 1 for
// n = 0..32 -- read out of .rdata, 33 words, 0, 1, 3, 7, ... FFFFFFFF.
// The `rlwinm r7,r4,2,0,29` + `lwzx r5,r7,r5` is that subscript, with the
// INDEX in rA, which is the shape a global array at offset 0 gives.
//
//   lwz r6,0(r3) / lwz r8,4(r3) / lwz r11,16(r3)   pos, bit, limit
//   addi r3,r6,4 ; cmpw cr6,r3,r11 ; blt- 825C0038   -> straight to the read
//   rlwinm r7,r6,3 ; rlwinm r11,r11,3 ; add r7,r7,r10
//   li r3,-1 ; cmpw cr6,r7,r11 ; bgt- 825C00B4       -> -1, but still advance
//
// THE FAILURE VALUE IS SET ON ONE PATH ONLY, and no `if`/`else` spelling of
// this reaches that. Four shapes were measured against the 236-byte target:
//
//   if (A && B) v = -1; else { read }        240 B,  9 of 59  -- MSVC inverts
//   if (!A || !B) { read } else v = -1;      240 B, 25 of 59     the second
//                                                                test to
//                                                                `ble-` into
//                                                                the read and
//                                                                needs a `b`
//                                                                to the tail
//   u32 v = -1; if (!A || !B) { read }       236 B, 25 of 59  -- right length,
//                                                                but the `li`
//                                                                is HOISTED
//                                                                into the
//                                                                entry block,
//                                                                which costs
//                                                                r3 as the
//                                                                scratch for
//                                                                pos+4 and
//                                                                renames every
//                                                                register
//                                                                after it
//   the goto form below                      236 B, 57 of 57
//
// The tell is that `li r3,-1` sits in the SECOND block, between that block's
// last operand and its compare. An initialiser before the `if` is live across
// the first branch, so MSVC sinks it only as far as the entry block; the
// target's value is dead unless the first test already failed. Writing the
// first guard as a jump INTO the read leaves the assignment where the image
// has it, and with r3 free at `addi r3,r6,4` the whole allocation falls out:
// `s` to r9, the end position to r10, the accumulator to r11.
//
// The member-function lever does not apply here -- `Read(int)` on BitReader
// compiles to the same 25 of 59.  Nor does `/O2 /Os`, which is 23 of 59.
//
// The read is a four-deep nest, each level guarded by the running end
// position against 8/16/24/32 -- `ble-` jumping forward to the shared
// `and r3,r11,r5`, so every level is written as the positive body and the
// mask is the single exit.
//
//   clrlwi r11,r8,24 ; srw r11,r4,r11        p[0] >> (u8)bit
//   subfic r3,r8,8   ; slw r4,r4,r3          p[1] << (8 - bit)
//
// The first shift count is narrowed to a BYTE and the later ones are not,
// which is why the cast is written only on the first term.
//
// The tail is shared by both exits and reloads the pointer, because the -1
// path never loaded it: `srawi r11,r10,3 ; addze r11,r11` is a SIGNED divide
// by 8, so the end position is an `int`, and `clrlwi r10,r10,29` beside it is
// a plain `& 7` rather than a signed remainder. Stores go 4, 12, 0.

#include "types.h"

struct BitReader
{
    /* 0x00 */ int       pos;
    /* 0x04 */ int       bit;
    /* 0x08 */ int       unk08;
    /* 0x0C */ const u8* ptr;
    /* 0x10 */ int       limit;
};

ASSERT_OFFSET(BitReader, bit, 0x04);
ASSERT_OFFSET(BitReader, ptr, 0x0C);
ASSERT_OFFSET(BitReader, limit, 0x10);

static const u32 kBitMask[33] =
{
    0x00000000u, 0x00000001u, 0x00000003u, 0x00000007u,
    0x0000000Fu, 0x0000001Fu, 0x0000003Fu, 0x0000007Fu,
    0x000000FFu, 0x000001FFu, 0x000003FFu, 0x000007FFu,
    0x00000FFFu, 0x00001FFFu, 0x00003FFFu, 0x00007FFFu,
    0x0000FFFFu, 0x0001FFFFu, 0x0003FFFFu, 0x0007FFFFu,
    0x000FFFFFu, 0x001FFFFFu, 0x003FFFFFu, 0x007FFFFFu,
    0x00FFFFFFu, 0x01FFFFFFu, 0x03FFFFFFu, 0x07FFFFFFu,
    0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu,
    0xFFFFFFFFu
};

u32 BitReaderRead(BitReader* s, int n)
{
    int pos = s->pos;
    int bit = s->bit;
    int limit = s->limit;
    int end = bit + n;
    u32 mask = kBitMask[n];
    u32 v;
    int adv;

    if (pos + 4 < limit)
        goto read;

    v = 0xFFFFFFFFu;
    if (pos * 8 + end > limit * 8)
        goto update;

read:
    {
        const u8* p = s->ptr;
        u32 acc = p[0] >> (u8)bit;

        if (end > 8)
        {
            acc |= (u32)p[1] << (8 - bit);

            if (end > 16)
            {
                acc |= (u32)p[2] << (16 - bit);

                if (end > 24)
                {
                    acc |= (u32)p[3] << (24 - bit);

                    if (end > 32 && bit != 0)
                        acc |= (u32)p[4] << (32 - bit);
                }
            }
        }

        v = acc & mask;
    }

update:
    adv = end / 8;
    s->bit = end & 7;
    s->ptr += adv;
    s->pos = pos + adv;
    return v;
}
