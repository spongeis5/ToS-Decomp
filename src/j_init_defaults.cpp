#include "types.h"

// sub_82202B50 -- initialise an object to its defaults. 116 B, 12 callers.
//
//   0x00 = &8200B1D0 (a vtable: its first word is the code address 8219E878)
//   0x24 = 0 ; 0x4C = 1 ; 0x04 0x08 0x0C 0x10 = 0 ; 0x14 = 1 ; 0x38 = 0 (byte)
//   0x30 = a byte BITFIELD whose first-declared 4-bit member is set to 11:
//          lbz / rlwimi rD,rS,4,0,27 / stb, which is what
//          `struct { u8 a : 4; u8 b : 4; }` with `a = 11` emits here --
//          MSVC allocates bitfields MSB-first on this big-endian target,
//          so the FIRST member is the high nibble. Verified both ways.
//   0x40 0x44 0x48 = 1.0f (82002D40)
//   0x28 = 8.0f (8200B1CC) ; 0x2C = 19.0f (8200B1C8)
//
// The two 8200B1xx constants sit immediately below the vtable, so they are
// this translation unit's own .rdata rather than the shared 82002xxx pool.
//
// NOT A MATCH: 5 of 29 words, and the size is exactly 116 in every variant
// tried. The instruction MULTISET is right -- four `lis`, one `addi`, three
// `li`, one `lbz`, one `rlwimi`, three `lfs`, fourteen stores -- and both
// store streams come out in the right relative order. What differs is where
// the SETUP sits: this source hoists the bitfield's `lbz` to the second
// instruction and loads all three float constants before any store, while the
// target issues the vtable store first, defers the `lbz` past six stores, and
// defers the 8.0f and 19.0f loads past the `rlwimi`. It also reuses f0 for
// 8.0f after the last 1.0f store where this allocates a third register.
//
// The search that says this is scheduling and not shape:
//
//   * ALL 2002 interleavings of the ten integer and five float store
//     statements (the two streams held in their target order) -- every one
//     is 5 of 29 at 116 bytes.
//   * The bitfield write at each of the 15 positions in three different
//     bases -- 5 of 29 or worse.
//   * Nine shapes: a real constructor of a class with a virtual function (so
//     MSVC emits its own pinned vfptr store), the same with a member
//     initialiser list, a free function, a free function returning `this`,
//     and the bitfield written as an explicit `(b & 0x0F) | 0xB0` mask
//     instead of a bitfield member. All 5 of 29; the constructor is
//     byte-identical to the free function, so the vfptr store is NOT pinned.
//   * All 72 flag combinations tools/flagsweep.py builds from the compiler's
//     own option list, including /Ou (prescheduling) and /Os: every one is
//     116 bytes and none beats 5 of 29.
//
// So the free choice here is instruction order between the setup block and
// the stores, which is the same family as the six entries in MATCHED.md's
// "What still resists".

extern "C" void* const kVTable[];

struct Bits { u8 hi : 4; u8 lo : 4; };

struct Defaults
{
    void* vt;
    s32   a04;
    s32   a08;
    s32   a0C;
    s32   a10;
    s32   a14;
    u8    pad18[0x0C];
    s32   a24;
    f32   a28;
    f32   a2C;
    Bits  b30;
    u8    pad31[0x07];
    u8    b38;
    u8    pad39[0x07];
    f32   a40;
    f32   a44;
    f32   a48;
    s32   a4C;
};
ASSERT_OFFSET(Defaults, a14, 0x14);
ASSERT_OFFSET(Defaults, a24, 0x24);
ASSERT_OFFSET(Defaults, a28, 0x28);
ASSERT_OFFSET(Defaults, b30, 0x30);
ASSERT_OFFSET(Defaults, b38, 0x38);
ASSERT_OFFSET(Defaults, a40, 0x40);
ASSERT_OFFSET(Defaults, a4C, 0x4C);

void InitDefaults(Defaults* p)
{
    p->vt = (void*)kVTable;
    p->a24 = 0;
    p->a4C = 1;
    p->a04 = 0;
    p->a08 = 0;
    p->a0C = 0;
    p->a10 = 0;
    p->b38 = 0;
    p->a14 = 1;
    p->b30.hi = 11;

    p->a40 = 1.0f;
    p->a44 = 1.0f;
    p->a48 = 1.0f;
    p->a28 = 8.0f;
    p->a2C = 19.0f;
}
