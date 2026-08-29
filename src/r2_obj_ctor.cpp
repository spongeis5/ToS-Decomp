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
// MEASUREMENT: see the note at the bottom of this file.

struct VT82700DF8;
extern const VT82700DF8 kVT_8207D1D8;
extern const VT82700DF8 kVT_8208B464;
extern const VT82700DF8 kVT_8207D308;
extern const VT82700DF8 kVT_8207D300;

struct Obj
{
    /* 0x000 */ const VT82700DF8* vt0;
    /* 0x004 */ void* f04;
    /* 0x008 */ u8    f08;
    /* 0x009 */ u8    pad09[3];
    /* 0x00C */ const VT82700DF8* vt0C;
    /* 0x010 */ u32   g0 : 4;
                u32   g1 : 1;
                u32   g2 : 5;
                u32   g3 : 22;
    /* 0x014 */ u8    h0 : 1;
                u8    h1 : 1;
                u8    h2 : 1;
                u8    h3 : 1;
                u8    h4 : 1;
                u8    h5 : 1;
                u8    h6 : 1;
                u8    h7 : 1;
    /* 0x015 */ u8    f15;
    /* 0x016 */ u8    i0 : 1;
                u8    i1 : 1;
                u8    i2 : 1;
                u8    i3 : 1;
                u8    i4 : 4;
    /* 0x017 */ u8    pad17;
    /* 0x018 */ char* f18;
    /* 0x01C */ s32   f1C;
    /* 0x020 */ f64   f20;
    /* 0x028 */ s32   f28;
    /* 0x02C */ u8    pad2C[0x187 - 0x2C];
    /* 0x187 */ char  buf[1];
};
ASSERT_OFFSET(Obj, vt0C, 0x0C);
ASSERT_OFFSET(Obj, f15,  0x15);
ASSERT_OFFSET(Obj, f18,  0x18);
ASSERT_OFFSET(Obj, f1C,  0x1C);
ASSERT_OFFSET(Obj, f20,  0x20);
ASSERT_OFFSET(Obj, f28,  0x28);
ASSERT_OFFSET(Obj, buf,  0x187);

void Construct(Obj* o, void* a, f64 t)
{
    o->vt0 = &kVT_8207D1D8;
    o->f04 = a;
    o->f08 = 0;

    o->vt0C = &kVT_8208B464;
    o->g0 = 0;
    o->g1 = 1;
    o->g2 = 1;
    o->h0 = 0;
    o->h1 = 1;
    o->h2 = 0;
    o->h3 = 0;
    o->h4 = 0;
    o->h5 = 0;
    o->h6 = 0;
    o->f15 = 0;
    o->i0 = 1;
    o->i1 = 0;
    o->i2 = 0;
    o->i3 = 0;
    o->f18 = 0;

    o->vt0 = &kVT_8207D308;
    o->vt0C = &kVT_8207D300;
    o->f18 = o->buf;
    o->f1C = 0;
    o->f20 = t;
    o->f28 = 0;
    o->buf[0] = 0;
    o->g0 = 3;
    o->g1 = 0;
}
