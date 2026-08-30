// sub_82578558 -- accept or reject a WAVEFORMATEX. 200 bytes of a 316-byte
// .pdata row (the five 24-byte adjust-and-tail-call thunks at 82578620
// onward share the unwind record), no direct callers.
//
// MATCHED at /O2: 48 of 48 non-relocated words, 200 bytes, with the row's
// extra 116 bytes reconciled by match.py's shrink proof.
//
// THIS TRANSLATION UNIT NAMES ITSELF. The two subformat constants sit at
// 82060780 and 82060790, and the very next bytes, at 820607A0, are the
// NUL-terminated string
//
//      ..\src\fmod_codec_wav.cpp
//
// so this is FMOD's WAV codec and the file it came from is not a guess. That
// is the second unit recovered this way, after `mod_dspi.cpp` at 8205E630.
//
// THE LAYOUT NAMES ITSELF TOO. The field at +0 is compared against 0xFFFE,
// which is WAVE_FORMAT_EXTENSIBLE, and against 1 and 3, which are
// WAVE_FORMAT_PCM and WAVE_FORMAT_IEEE_FLOAT; +14 is read as a halfword and
// tested against 8; +24 is compared 16 bytes at a time against two adjacent
// constants. That is WAVEFORMATEXTENSIBLE exactly -- wFormatTag at 0,
// wBitsPerSample at 14, SubFormat at 24 -- so the fields are named rather
// than described. The constants read, big-endian as this target stores a
// GUID, as KSDATAFORMAT_SUBTYPE_PCM and KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
// their bytes are copied here from the image rather than invented.
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
// ---- the three words that were wrong, and what they were ----
//
// The second inlined memcmp's three setup instructions came out ROTATED:
//
//      want  addi r10,r6,16 ; mr r11,r9 ; addi r8,r9,16
//      got   mr r11,r9 ; addi r8,r9,16 ; addi r10,r6,16
//
// -- retail forms the second constant's address FIRST; every spelling that
// declares the pair as ONE two-element array groups the two values derived
// from `w->subFormat` instead and forms it last.
//
// **THE TWO CONSTANTS ARE TWO SEPARATE SAME-TU STATICS, NOT A TWO-ELEMENT
// ARRAY, AND THAT IS WHAT THE ROTATION SAYS.** As an array element the
// second address is `base + 16` and the scheduler treats it as one more
// value derived from the first; as its own STATIC it is a distinct symbol
// whose address MSVC still emits as `addi r10,r6,16`, because it knows the
// two objects are 16 bytes apart in one section -- the same relative-offset
// knowledge that decides sub_82158E50 and sub_821FA140 -- but it now heads
// the block. Same three instructions, same registers, different order, and
// the rest of the function is untouched: 45 of 48 to 48 of 48.
//
// The declaration ORDER is load-bearing and is therefore evidence: with the
// IEEE_FLOAT constant declared first the score drops to 46 of 48 and a
// second word moves, so PCM is the lower address, which is also what the
// image shows. A `struct Guid { u8 b[16]; }` pair is byte-identical to two
// `u8[16]`s, so the bytes do not choose between those two spellings.
//
// This was not reachable from the code: 32 source shapes were measured
// against the rotation -- named locals for either pointer in either
// declaration order, `kSubFormats[0] + 16`, `g += 16`, an array-of-array
// pointer, swapped memcmp arguments, nested ifs, `||` of equalities, `goto
// ok`, results in locals, `sizeof` for the count, `void*` casts, four
// inlined-helper forms with the parameters both ways round and one taking
// the index, a re-derived `(const u8*)w + 24`, and three loop forms (which
// do not unroll: 160 to 180 bytes) -- and all 2304 combinations of
// `flagsweep.py --full`. Every one of them is 45 of 48 with the SAME three
// words. The declaration of the constants was the only thing that moved it.
//
// What the shape of the tests had to be, and it is still the interesting
// half:
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
//   if (w == 0) return 45;                               the shape below
//
// So retail keeps TWO copies of `li r3,45 ; blr` and the third site branches
// to the first: the null check and the subformat failure share one, the tag
// failure owns the other. Only the flat `if`/`else if` chain with each
// rejection as its own early return produces that; a nested `if (w != 0)`
// with the rejections inside merges all three, which is 8 bytes shorter and
// the wrong shape.

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

/* 82060780 and 82060790, byte for byte out of the image. A GUID is stored
 * big-endian on this target, so these read as {00000001-0000-0010-8000-
 * 00AA00389B71} and {00000003-...}: KSDATAFORMAT_SUBTYPE_PCM and
 * KSDATAFORMAT_SUBTYPE_IEEE_FLOAT. PCM must be declared first -- see the
 * note above; the order is measurable in the code, not just in the data. */
static const u8 kSubFormatPcm[16] =
{
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10,
    0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

static const u8 kSubFormatFloat[16] =
{
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10,
    0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

int CheckWaveFormat(FormatHolder* h)
{
    WaveFormatEx* w = h->format;

    if (w == 0)
        return 45;

    if (w->wFormatTag == 0xFFFE)
    {
        if (memcmp(w->subFormat, kSubFormatPcm, 16) != 0
            && memcmp(w->subFormat, kSubFormatFloat, 16) != 0)
            return 45;
    }
    else if (w->wFormatTag != 1 && w->wFormatTag != 3)
    {
        return 45;
    }

    return w->wBitsPerSample == 8 ? 45 : 0;
}
