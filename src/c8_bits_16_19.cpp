// sub_8224DBB0 -- two flag tests on one 32-bit word: true when the first bit
// is set and the second is clear. 40 bytes, 5 callers.
//
//      lwz     r11,468(r3)         +0x1D4, read ONCE
//      rlwinm  r10,r11,16,31,31    the 0x00010000 bit, NORMALISED to 0/1
//      cmplwi  cr6,r10,0
//      beq-    cr6,zero
//      rlwinm  r11,r11,13,31,31    the 0x00080000 bit, normalised to 0/1
//      li      r3,1
//      cmplwi  cr6,r11,0
//      beqlr   cr6                 second bit clear -> return 1
// zero:li      r3,0
//      blr
//
// rlwinm rD,rS,SH,31,31 keeps the bit that rotate-left-SH brings to the LSB,
// which is value bit (32 - SH) & 31: SH=16 is bit 16, SH=13 is bit 19.
//
// THE WHOLE FUNCTION IS THE INLINED-`bool`-HELPER LEVER, and it is worth
// stating as a measurement because eight of the ten words are right without
// it. Written with the tests inline -- either as `e->flags & 0x10000` or as
// `(e->flags >> 16) & 1`, and either as a plain mask or as a one-bit
// bitfield -- MSVC masks the bit IN PLACE and never rotates it down:
//
//      want  rlwinm r10,r11,16,31,31      got  rlwinm r10,r11,0,15,15
//      want  rlwinm r11,r11,13,31,31      got  rlwinm r11,r11,0,12,12
//
// 8 of 10 words, with every branch, every `li` and both `cmplwi`s already
// identical. Nothing about the branch shape is wrong; the compiler simply
// has no reason to build a 0/1 when only zero-versus-nonzero is wanted.
//
// A `bool`-returning accessor gives it one. The inlined helper must produce
// a genuine `bool`, so the bit is normalised to 0/1 and only then tested --
// which is exactly the pair of rotates the target has. 10 of 10.
//
// This is the same lever MATCHED.md records as "a materialised-then-masked
// bool is an inlined helper", reached from the other end: there the tell is
// a redundant `clrlwi ...,24` before the test, here the normalisation is
// free inside the `rlwinm` itself, so the tell is the ROTATE AMOUNT rather
// than an extra instruction.
//
// What is NOT decided: the accessors' bodies. `(flags & 0x10000) != 0` and a
// one-bit unsigned bitfield at the same position compile to the same ten
// words, so this file cannot say which the retail source used, and the
// bitfield spelling is not recorded here because it would assert a layout
// the bytes do not establish. `return HasA() && !HasB();` is also 10 of 10;
// the nested form below is written because it reads straight off the
// branches.
//
// The return is NOT `bool` -- there is no trailing `clrlwi` and the 0/1 goes
// straight into r3 (MATCHED.md). That reads backwards, and it is the mirror
// image of src/c7_ready_flag.cpp in this same batch, which does have the
// mask and does return `bool`.
//
// Nothing is relocated; all 10 words are compared.

#include "types.h"

struct Entry
{
    /* 0x000 */ char unk0000[0x1D4];
    /* 0x1D4 */ u32  flags;

    bool HasA() const { return (flags & 0x00010000) != 0; }
    bool HasB() const { return (flags & 0x00080000) != 0; }
};

ASSERT_OFFSET(Entry, flags, 0x1D4);

int IsAvailable(Entry* e)
{
    if (e->HasA())
    {
        if (!e->HasB())
            return 1;
    }
    return 0;
}
