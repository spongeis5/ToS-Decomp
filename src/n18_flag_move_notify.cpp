// sub_821FC180 -- move one bit between two byte flag words, and notify when
// the result leaves two other bits clear. 128 B, 4 callers.
//
//      lbz    r11,116(r3)        a = o->f74
//      rlwinm r10,r11,0,25,25    a & 0x40      -- NO ROTATE
//      cmplwi cr6,r10,0
//      beq-   cr6,plain
//      lbz    r10,117(r3)        b = o->f75
//      rlwinm r9,r4,6,25,25      (v & 1) << 6
//      clrlwi r8,r11,24
//      or     r7,r9,r10
//      rlwinm r8,r8,0,26,24      a & ~0x40
//      clrlwi r6,r7,24
//      stb    r8,116(r3)         o->f74 = a & ~0x40
//      rlwinm r5,r6,0,26,26      nb & 0x20
//      stb    r6,117(r3)         o->f75 = nb
//      cmplwi cr6,r5,0
//      bnelr  cr6
//      clrlwi r11,r6,24
//      rlwinm r10,r11,0,25,25    nb & 0x40
//      cmplwi cr6,r10,0
//      bnelr  cr6
//      lis    r6,-17554 ; lwz r5,156(r3) ; li r8,1 ; li r7,0
//      ori    r6,r6,11277        = BB6E2C0D
//      li     r4,0 ; li r3,0
//      b      0x8217E048
// plain:
//      lbz    r11,117(r3)
//      rlwinm r10,r4,6,25,25
//      or     r9,r10,r11
//      stb    r9,117(r3)         o->f75 |= (v & 1) << 6
//      blr
//
// EVERY BIT TEST IS AN INLINE MASK, NOT AN ACCESSOR. MATCHED.md's rule is
// that the ROTATE AMOUNT is the tell: `rlwinm rD,rS,0,25,25` leaves the bit
// where it is and is what `flags & 0x40` compiles to, whereas a
// bool-returning accessor would rotate it down to bit 31 with
// `rlwinm rD,rS,25,31,31`. All four tests here have rotate 0, so all four are
// written in place. Decoding the masks: 25,25 is bit 6 (0x40), 26,26 is bit 5
// (0x20), and 26,24 WRAPS -- bits 26..31 and 0..24, every bit except bit 6 --
// so it is `& ~0x40`.
//
// THE SHIFT MASK SAYS THE BIT IS MASKED IN THE SOURCE, AND `bool` IS NOT
// ENOUGH. `rlwinm r9,r4,6,25,25` rotates left 6 and keeps ONLY bit 6, so only
// bit 0 of r4 survives. A `bool` parameter shifted by 6 does not do that: it
// gives `rlwinm r9,r4,6,18,25`, keeping EIGHT bits, because MSVC narrows a
// bool to its byte width and not to one bit -- 29 of 31, and the same two
// words wrong as a plain `int` and as a one-bit bitfield member. The mask has
// to be written: `(v & 1) << 6` is 31 of 31.
//
// The parameter's type is then unreadable -- `bool`, `int`, `u8`, `u32` and
// `(u8)v & 1` are byte-identical once the `& 1` is there, and so is
// `(v << 6) & 0x40`. `bool` is kept because the callers pass a two-valued
// thing and the mask reads as defensive rather than as arithmetic; nothing
// here proves it.
//
// A ternary (`v ? 0x40 : 0`) is 156 bytes and `(v != 0) << 6` is 136, so both
// are ruled out by size rather than by taste.
//
// THE REDUNDANT `clrlwi ...,24` PAIRS ARE READ-BACKS. r11 comes from an `lbz`
// and is already zero-extended, so `clrlwi r8,r11,24` computes nothing -- it
// is there because the value being masked is a `u8` field's, and the same
// happens at `clrlwi r6,r7,24` and again at `clrlwi r11,r6,24`. The third one
// is the giveaway: the two guards re-read `o->f75` after the `stb`, and MSVC
// forwards the stored word through the load, re-applying the field's own
// truncation each time. So the two tests are written against the FIELD, not
// against a local holding the new value.
//
// The `|=` on +0x75 appears in BOTH arms, and the else arm's copy has no
// `clrlwi` after its `or` because nothing reads it back.
//
// The tail call takes six arguments -- (0, 0, o->f9C, 0xBB6E2C0D, 0, 1) --
// and 0xBB6E2C0D is built with `lis`+`ori`, so it is a plain 32-bit literal
// and not an address: an address in this image would be `lis`+`addi` with a
// relocation, and this word carries none.
//
// The /Os build is 112 bytes, so the level is settled by size here rather
// than by register naming.
//
// Only the `b` is relocated; the other 31 words are compared.

#include "types.h"

struct FlagObj
{
    /* 0x00 */ char  unk0000[0x74];
    /* 0x74 */ u8    f74;
    /* 0x75 */ u8    f75;
    /* 0x76 */ char  unk0076[0x26];
    /* 0x9C */ void* f9C;
};
ASSERT_OFFSET(FlagObj, f74, 0x74);
ASSERT_OFFSET(FlagObj, f75, 0x75);
ASSERT_OFFSET(FlagObj, f9C, 0x9C);

void Notify(int a, int b, void* c, u32 tag, int d, int e);

void MoveFlagAndNotify(FlagObj* o, bool v)
{
    if (o->f74 & 0x40)
    {
        o->f74 &= ~0x40;
        o->f75 |= (v & 1) << 6;

        if (o->f75 & 0x20)
            return;
        if (o->f75 & 0x40)
            return;

        Notify(0, 0, o->f9C, 0xBB6E2C0D, 0, 1);
    }
    else
    {
        o->f75 |= (v & 1) << 6;
    }
}
