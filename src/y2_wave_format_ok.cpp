// sub_82578558 -- accept or reject a WAVEFORMATEX. 200 bytes of a 316-byte
// .pdata row (the five 24-byte adjust-and-tail-call thunks at 82578620
// onward share the unwind record), no direct callers.
//
// THE LAYOUT NAMES ITSELF. The field at +0 is compared against 0xFFFE,
// which is WAVE_FORMAT_EXTENSIBLE, and against 1 and 3, which are
// WAVE_FORMAT_PCM and WAVE_FORMAT_IEEE_FLOAT; +14 is read as a halfword and
// tested against 8; +24 is compared 16 bytes at a time against two adjacent
// constants. That is WAVEFORMATEXTENSIBLE exactly -- wFormatTag at 0,
// wBitsPerSample at 14, SubFormat at 24 -- so the fields are named rather
// than described.
//
// The two constants are ONE symbol, not two: the second compare's base is
// `addi r10,r6,16`, derived from the first, and MSVC does not do that across
// two independent symbols. So 82060780 holds an array of two 16-byte
// subformat GUIDs.
//
//      mr    r11,r9 ; mr r10,r6 ; addi r7,r9,16
//   L: lbz   r8,0(r11) ; lbz r4,0(r10) ; subf. r8,r4,r8 ; bne- out
//      addi  r11,r11,1 ; addi r10,r10,1
//      cmpw  cr6,r11,r7 ; bne+ cr6,L
//  out: cmpwi cr6,r8,0 ; beq- cr6,ok
//
// -- both pointers increment and the difference is a `subf` of two `lbz`s,
// which per MATCHED.md is the `memcmp`/`strcmp` INTRINSIC rather than a
// hand-written body; a hand-written one gets the loop-invariant-delta
// transform instead.
//
// The tail is branchless:
//
//      addi  r9,r11,-8 ; addic r8,r9,-1 ; subfe r6,r7,r7 ; and r3,r6,r10
//
// with r10 = 45, so it selects 45 when wBitsPerSample == 8 and 0 otherwise
// -- the `addic`/`subfe` idiom carrying a value instead of a 0/1.
//
// NEAR MISS: 45 of 50 words at /O2, correct length once the shape is right,
// and the three remaining differences are a ROTATION of the second
// `memcmp`'s three setup instructions:
//
//      want  addi r10,r6,16 ; mr r11,r9 ; addi r8,r9,16
//      got   mr r11,r9 ; addi r8,r9,16 ; addi r10,r6,16
//
// -- retail forms the second GUID's address FIRST; we group the two values
// derived from `w->subFormat` and form it last. Nothing else in the
// function differs.
//
// What the shape had to be, and it is the interesting half:
//
//   if (w != 0) { ...; return 45; } return 45;           192 B, 32 of 48 --
//                                                        MSVC tail-merges
//                                                        all THREE `li 45 ;
//                                                        blr` sites into one
//                                                        and the tag test
//                                                        branches backward
//                                                        into it
//   the same with the tag arm moved into an `else`       192 B, 32 of 48 --
//                                                        identical merge
//   if (w == 0) return 45;                               200 B, 45 of 50
//   if (tag == 0xFFFE) { if (memcmp(a) && memcmp(b))
//                            return 45; }
//   else if (tag != 1 && tag != 3) return 45;
//   return bits == 8 ? 45 : 0;
//
// So retail keeps TWO copies of `li r3,45 ; blr` and the third site branches
// to the first: the null check and the subformat failure share one, the tag
// failure owns the other. Only the flat `if`/`else if` chain with each
// rejection as its own early return produces that; a nested `if (w != 0)`
// with the rejections inside merges all three, which is 8 bytes shorter and
// the wrong shape.
//
// Four spellings of the second GUID address were tried against the rotation
// and none moved it: `kSubFormats[1]`, a named `const u8* alt` declared
// ahead of the condition, `g += 16` as its own statement between the two
// compares, and `/O2 /Os` (which is 4 of 48 and a different function).

#include "types.h"
#include <string.h>

struct WaveFormatEx
{
    /* 0x00 */ u16 wFormatTag;
    /* 0x02 */ u16 nChannels;
    /* 0x04 */ u32 nSamplesPerSec;
    /* 0x08 */ u32 nAvgBytesPerSec;
    /* 0x0C */ u16 nBlockAlign;
    /* 0x0E */ u16 wBitsPerSample;
    /* 0x10 */ u16 cbSize;
    /* 0x12 */ u16 samples;
    /* 0x14 */ u32 dwChannelMask;
    /* 0x18 */ u8  subFormat[16];
};
ASSERT_OFFSET(WaveFormatEx, wBitsPerSample, 0x0E);
ASSERT_OFFSET(WaveFormatEx, subFormat,      0x18);

struct FormatHolder
{
    /* 0x000 */ u8            unk0000[0x160];
    /* 0x160 */ WaveFormatEx* format;
};
ASSERT_OFFSET(FormatHolder, format, 0x160);

extern const u8 kSubFormats[2][16];      /* 82060780 */

int CheckWaveFormat(FormatHolder* h)
{
    WaveFormatEx* w = h->format;

    if (w == 0)
        return 45;

    if (w->wFormatTag == 0xFFFE)
    {
        if (memcmp(w->subFormat, kSubFormats[0], 16) != 0
            && memcmp(w->subFormat, kSubFormats[1], 16) != 0)
            return 45;
    }
    else if (w->wFormatTag != 1 && w->wFormatTag != 3)
    {
        return 45;
    }

    return w->wBitsPerSample == 8 ? 45 : 0;
}
