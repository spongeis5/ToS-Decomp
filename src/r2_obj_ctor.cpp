#include "types.h"

// sub_82700DF8 -- a constructor. 168 B, 8 callers, one double argument.
//
// TWO vptrs, each WRITTEN TWICE: +0 gets 8207D1D8 and later 8207D308, and
// +12 gets 8208B464 and later 8207D300. Two bases each install their own
// vtable and the most-derived class then overwrites both, which is the shape
// of `struct D : A, B` where B's subobject starts at +12. The derived pair
// 8207D300 / 8207D308 are 8 bytes apart -- one class's two vtables, adjacent.
//
//      std     r31,-8(r1)      no frame; the red zone holds one callee-save
//      lis     r10,-32248
//      stw     r4,4(r3)        A: f04 = arg
//      li      r11,0
//      addi    r9,r10,-11816   = 8207D1D8
//      stb     r11,8(r3)       A: f08 = 0   (a BYTE)
//      li      r10,1
//      stw     r9,0(r3)        A: vptr      (slipped past two stores)
//      li      r9,33
//      lbz     r6,20(r3)
//      lis     r7,-32247
//      lbz     r4,22(r3)
//      lis     r5,-32248
//      lwz     r8,16(r3)
//      rlwimi  r8,r9,22,0,9    +16 bits 0..9  := 33
//      rlwimi  r4,r10,7,0,27   +22 bits 0..3  := 8      (of the byte)
//      stb     r11,21(r3)      +21 = 0
//      rlwimi  r6,r10,6,0,30   +20 bits 0..6  := 0x40   (of the byte)
//      stw     r11,24(r3)      B: f24 = 0
//      addi    r9,r7,-19356    = 8208B464
//      stw     r8,16(r3)
//      lis     r31,-32248
//      stb     r4,22(r3)
//      stb     r6,20(r3)
//      addi    r10,r3,391
//      stw     r9,12(r3)       B: vptr
//      addi    r8,r5,-11512    = 8207D308
//      addi    r9,r31,-11520   = 8207D300
//      stw     r10,24(r3)      D: f24 = this + 391
//      li      r7,3
//      stw     r11,28(r3)      D: f28 = 0
//      stw     r11,40(r3)      D: f40 = 0
//      stfd    f1,32(r3)       D: f32 = the double argument
//      stb     r11,391(r3)     D: buf[0] = 0
//      addi    r10,r3,16       DEAD -- never read
//      stw     r8,0(r3)        D: vptr
//      stw     r9,12(r3)       D: vptr2
//      lwz     r11,16(r3)
//      rlwimi  r11,r7,28,0,4   +16 bits 0..4 := 6, OVERWRITING part of the 33
//      stw     r11,16(r3)
//      ld      r31,-8(r1)
//
// READING THE THREE `rlwimi`s. For a bitfield insert the mask is the field
// and SH is 31 - ME, so the encoding states the layout outright:
//
//   (SH 22, MB 0, ME 9) -> width 10 at the TOP of the +16 word, value 33.
//        31 - 9 == 22, so this one is a single exact group.
//   (SH 28, MB 0, ME 4) -> bits 0..4 of the same word, value rotl(3,28)
//        = 6.  31 - 4 == 27, NOT 28, so it is not one field: it is a 4-bit
//        field at bits 0..3 set to 3 (SH 28 is that field's own position)
//        MERGED with the 1-bit field at bit 4 set to 0.
//   (SH 7, MB 0, ME 27) and (SH 6, MB 0, ME 30) are byte inserts. MB 0 is
//        free because the result leaves through `stb`, so only 24..31 matter;
//        ME is not, and it says how far right the assigned group reaches --
//        four bits of the +22 byte, seven of the +20 byte.
//
// Since 33 = 0b0000100001 over bits 0..9 and the later write covers bits
// 0..4 only, the word at +16 splits 4 / 1 / 5 / 22: the base sets (0, 1, 1)
// and the derived resets the first two to (3, 0).
//
// MEASURED: 18 of 34 non-relocated words at /O2 /Os, at the EXACT size of
// 168 bytes -- up from 1 of 26 at 120.
//
// THE "INLINING BOUNDARY A SINGLE TRANSLATION UNIT CANNOT EXPRESS" WAS
// WRONG. README.md listed this function, sub_82583290 and sub_82700B30
// together as needing one, on the grounds that MSVC's dead-store elimination
// removes duplicated stores the retail build keeps. The observation is
// right -- written flat, all three duplicated stores go and the body is 120
// bytes -- and the conclusion was not: a REAL BASE-CLASS HIERARCHY is that
// boundary, and it lives inside one translation unit.
//
//     struct A { void* f04; u8 f08; virtual void va(); };          // at +0
//     struct B { Flags g; ...bits...; char* f18; virtual void vb(); };
//     struct D : A, B { ... };                                     // B at +12
//
// A's constructor writes A's vptr, B's writes B's, and D's writes BOTH again
// -- the pair 8207D308 / 8207D300, eight bytes apart, which this file had
// already read as one class's two vtables. `f18 = 0` in B's body and
// `f18 = buf` in D's are likewise two different functions' stores, so DSE
// keeps them. That is 160 bytes, from 120.
//
// THE LAST EIGHT BYTES ARE AN INLINED HELPER ON THE BITFIELD WORD. The
// target carries a DEAD `addi r10,r3,16` -- the address of the +16 bitfield
// word, formed and never read -- which MATCHED.md records as the fingerprint
// of an inlined helper whose pointer argument was folded into displacements.
// Giving the derived class's second bitfield write its own helper
//
//     static void SetG(Flags* f) { f->g0 = 3; f->g1 = 0; }
//
// restores the address computation AND forces the +16 word to be stored and
// RELOADED for that write rather than kept live in a register: 168 bytes,
// the exact size, and 18 of 34. A bare `Flags* f = &g;` local does the same;
// so do a `Flags&` parameter, a helper taking `B*`, and two levels of
// helper. All five are 18 of 34, so the lever is the pointer, not the call.
//
// WHAT IS STILL WRONG, all of it in the middle third: our dead address is
// `this+12` where the target's is `this+16` (MSVC folds our `&g` back to the
// B subobject's base); B's vptr store is issued six slots early; the +22
// byte is read into r31 where the target uses r4, the parameter register it
// has finished with; and the two derived vtable addresses land in swapped
// registers. Three orderings of B's constructor body were measured
// (g/h/f15/i, h/f15/i/g, g/i/f15/h) at 18, 17 and 17, so the order already
// here is the best of them.
//
// FLAGS: /O2 /Os. Plain /O2 is 15 of 34 at the same size -- three registers
// coalesced differently, the usual /Os signature.


struct VT82700DF8;
extern const VT82700DF8 kVT_8207D1D8;
extern const VT82700DF8 kVT_8208B464;
extern const VT82700DF8 kVT_8207D308;
extern const VT82700DF8 kVT_8207D300;

/* The +16 word, as one bitfield group. Its address is what the target's dead
   `addi r10,r3,16` computes, so it is a thing the source names. */
struct Flags
{
    u32 g0 : 4;
    u32 g1 : 1;
    u32 g2 : 5;
    u32 g3 : 22;
};

/* Base at +0: its vtable is 8207D1D8 and D replaces it with 8207D308. */
struct A
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ void* f04;
    /* 0x08 */ u8    f08;
    /* 0x09 */ u8    pad09[3];

    A(void* a);
    virtual void va();
};

/* Base at +12: its vtable is 8208B464 and D replaces it with 8207D300. */
struct B
{
    /* 0x0C */ /* vptr */
    /* 0x10 */ Flags g;
    /* 0x14 */ u8    h0 : 1;
               u8    h1 : 1;
               u8    h2 : 1;
               u8    h3 : 1;
               u8    h4 : 1;
               u8    h5 : 1;
               u8    h6 : 1;
               u8    h7 : 1;
    /* 0x15 */ u8    f15;
    /* 0x16 */ u8    i0 : 1;
               u8    i1 : 1;
               u8    i2 : 1;
               u8    i3 : 1;
               u8    i4 : 4;
    /* 0x17 */ u8    pad17;
    /* 0x18 */ char* f18;

    B();
    virtual void vb();
};

struct D : A, B
{
    /* 0x1C */ s32  f1C;
    /* 0x20 */ f64  f20;
    /* 0x28 */ s32  f28;
    /* 0x2C */ u8   pad2C[0x187 - 0x2C];
    /* 0x187 */ char buf[1];

    D(void* a, f64 t);
    virtual void va();
    virtual void vb();
};

ASSERT_OFFSET(A, f04, 0x04);
ASSERT_OFFSET(A, f08, 0x08);
ASSERT_SIZE(A, 0x0C);
ASSERT_OFFSET(B, f15, 0x09);
ASSERT_OFFSET(B, f18, 0x0C);
ASSERT_SIZE(B, 0x10);

A::A(void* a)
{
    f04 = a;
    f08 = 0;
}

B::B()
{
    g.g0 = 0;
    g.g1 = 1;
    g.g2 = 1;
    h0 = 0;
    h1 = 1;
    h2 = 0;
    h3 = 0;
    h4 = 0;
    h5 = 0;
    h6 = 0;
    f15 = 0;
    i0 = 1;
    i1 = 0;
    i2 = 0;
    i3 = 0;
    f18 = 0;
}

/* The dead `addi r10,r3,16` says this write went through a POINTER to the
   bitfield word rather than through `this`, and that is also what forces the
   store-and-reload of +16 that the target does and a register-resident value
   would not. */
static void SetG(Flags* f)
{
    f->g0 = 3;
    f->g1 = 0;
}

D::D(void* a, f64 t) : A(a), B()
{
    f18 = buf;
    f1C = 0;
    f20 = t;
    f28 = 0;
    buf[0] = 0;
    SetG(&g);
}
