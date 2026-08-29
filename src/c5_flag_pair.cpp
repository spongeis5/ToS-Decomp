// sub_82761AD0 and sub_82761AF8 -- a matched pair of set-once flag methods on
// one object. 36 bytes each, 5 callers each.
//
// Merged into one translation unit and onto ONE struct, on evidence:
//
//   * they sit 4 bytes apart (82761AD0 + 36 = 82761AF4, next starts 82761AF8)
//   * both take the same pointer in r3
//   * both read and write the SAME two bytes, +0x14D and +0x14F, and both
//     set the same 0x02 bit in +0x14F
//
// sub_82761AD0, 36 bytes:
//
//      lbz     r11,333(r3)         +0x14D
//      rlwinm. r10,r11,0,30,30     r11 & 0x02, recording
//      bnelr                       already set -> return
//      lbz     r10,335(r3)         +0x14F
//      ori     r11,r11,2
//      ori     r10,r10,2
//      stb     r11,333(r3)
//      stb     r10,335(r3)
//      blr
//
// sub_82761AF8 is the same nine instructions with 0x08 in place of the first
// 0x02, both in the test and in the first `ori`:
//
//      rlwinm. r10,r11,0,28,28     r11 & 0x08
//      ori     r11,r11,8
//
// `rlwinm.` -- the RECORDING form, with the mask applied straight into CR0
// and no separate compare -- is a plain `x & bit` test, not a bitfield read.
// A bitfield extracts to a 0/1 value with a non-recording `rlwinm` and then
// compares it (compare src/c8_bits_16_19.cpp, which does exactly that).
//
// `bnelr` is the guard-as-conditional-return idiom, so the body is the
// fall-through and the early exit is written first.
//
// The second byte is loaded BEFORE either `ori`, which is scheduling: the
// load has the longer latency and nothing stores until both are in hand.
//
// BOTH NEED `/O2 /Os`, and the two symptoms at plain `/O2` are the ones
// MATCHED.md already names. At `/O2` this exact source is 44 bytes and 1 of
// 9 words:
//
//   * the recording mask splits into `rlwinm` + `cmplwi cr6` + `bnelr cr6`
//     -- the same `clrlwi.` versus `clrlwi`/`cmplwi` split that is the whole
//     of sub_827156B8's flag, and it is a property of the level, not a
//     source shape;
//   * an extra `clrlwi r9,r11,24` appears re-zero-extending a byte that
//     `lbz` already zero-extended, and the two `ori`s pick fresh registers
//     r7/r8 where `/Os` coalesces them onto r10/r11 -- the /Os
//     register-coalescing signature.
//
// At `/O2 /Os` both are 9 of 9. Two adjacent functions agreeing on the level
// is one more informative pair for the translation-unit claim.
//
// Nothing is relocated; all 9 words of each are compared.

#include "types.h"

struct Panel
{
    /* 0x000 */ char unk0000[0x14D];
    /* 0x14D */ u8   flags14D;
    /* 0x14E */ char unk014E[0x01];
    /* 0x14F */ u8   flags14F;
};

ASSERT_OFFSET(Panel, flags14D, 0x14D);
ASSERT_OFFSET(Panel, flags14F, 0x14F);
// No ASSERT_SIZE: nothing here bounds the object, and a guessed size would
// compile happily while being wrong.

// sub_82761AD0
void MarkDirty2(Panel* p)
{
    if (p->flags14D & 2)
        return;
    p->flags14D |= 2;
    p->flags14F |= 2;
}

// sub_82761AF8
void MarkDirty8(Panel* p)
{
    if (p->flags14D & 8)
        return;
    p->flags14D |= 8;
    p->flags14F |= 2;
}
