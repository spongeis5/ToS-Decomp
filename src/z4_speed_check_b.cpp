#include "types.h"

// sub_822553D8 -- 48 bytes, and the twin of sub_8219FCA8
// (src/z4_speed_check_a.cpp) in a different translation unit.
//
//      mflr    r12
//      stw     r12,-8(r1)
//      stwu    r1,-96(r1)
//      lis     r11,-32255        0x82010000
//      lwz     r3,8(r3)          arg1 = t->deep
//      lfs     f1,-9124(r11)     0x8200DC5C, whose word is 3E051EB8 = 0.13f
//      bl      0x82254A48
//      clrlwi  r3,r3,24
//      addi    r1,r1,96
//      lwz     r12,-8(r1)
//      mtlr    r12
//      blr
//
// Instruction for instruction the same function as its twin; only the two
// relocated addresses differ.  Its callee at 82254A48 is a 60-byte predicate
// with the same body as the twin's callee at 8219F618 and one different
// constant -- 0.3f at 82004768 against 1.0f at 82002D40 -- so the two are
// separate functions of the same shape, not one COMDAT shared by two callers.
// The pointer at +8 is the same field its neighbour sub_82255408
// (src/h_kind_allows.cpp, 0x30 later) reads.
//
// The narrowing reading is the twin's, measured there: an 8-bit return type
// over a 32-bit result is what produces the trailing `clrlwi` and, with it,
// the frame -- a `bool` callee makes this a 16-byte tail call instead.
//
// The constant is likewise a NAMED constant rather than a `0.13f` literal.
// Spelled as a literal, both files emit `__real@3e051eb8` and build.py
// refuses the pair -- one COMDAT name cannot resolve to both 82003A78 and
// 8200DC5C.  The .rdata around each address is a near-copy of the other and
// contains adjacent repeats of 5.0 and 2.0, which a folded literal pool
// cannot; the twin's comment carries that evidence in full.
//
// Three of the twelve words are relocated -- the lis/lfs pair addressing the
// constant, and the bl -- so 9 of 12 are compared.

struct SpeedDeep2;

int SpeedBelow2(SpeedDeep2* d, f32 limit);

struct SpeedTop2
{
    /* 0x00 */ char        unk0000[8];
    /* 0x08 */ SpeedDeep2* deep;
};
ASSERT_OFFSET(SpeedTop2, deep, 0x08);

extern const f32 kFloat_8200DC5C;   /* 3E051EB8 == 0.13f */

u8 IsSlow2(SpeedTop2* t)
{
    return SpeedBelow2(t->deep, kFloat_8200DC5C);
}
