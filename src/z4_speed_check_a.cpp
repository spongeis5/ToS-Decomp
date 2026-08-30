#include "types.h"

// sub_8219FCA8 -- 48 bytes.  Pull the deep object out of the holder and hand
// it, with a constant read from .rdata, to the 60-byte predicate at 8219F618.
//
//      mflr    r12
//      stw     r12,-8(r1)
//      stwu    r1,-96(r1)
//      lis     r11,-32256        0x82000000
//      lwz     r3,8(r3)          arg1 = t->deep
//      lfs     f1,14968(r11)     0x82003A78, whose word is 3E051EB8 = 0.13f
//      bl      0x8219F618
//      clrlwi  r3,r3,24
//      addi    r1,r1,96
//      lwz     r12,-8(r1)
//      mtlr    r12
//      blr
//
// The pointer at +8 is the same field src/i_state_idle.cpp (sub_8219FCD8,
// 0x30 later) and src/eq1_2260.cpp read, so this is that translation unit's
// holder.  No locals are allocated and nothing is spilled, so the whole
// 96-byte frame is linkage plus the outgoing parameter save area.
//
// THE TRAILING MASK IS WHY THERE IS A FRAME AT ALL.  With the callee declared
// `bool` this is a tail call -- 16 bytes, `lis`/`lwz`/`lfs`/`b`, no frame and
// no mask, measured.  The mask appears only when the returned value is 32
// bits wide and this function's return type is 8 bits, and then the call can
// no longer be a tail call because there is work after it.  So what is
// measured here is a NARROWING: a 32-bit result returned through an 8-bit
// return type.  `u8` and an explicit `(u8)` cast are byte-identical; `bool`
// is not -- `int`-to-`bool` is a `!= 0` test and emits `addic`/`subfe`
// instead of the mask, 4 of 12 words.
//
// Worth recording rather than smoothing over: the callee's OWN body ends
// `li r11,0/1 ; clrlwi r3,r11,24`, which is MATCHED.md's signature for a
// `bool` return.  The two readings disagree, and only the second is a
// measurement made against these bytes.  Something in the real source made
// the call site see a 32-bit return; this file does not claim to know what.
//
// THE FLOAT IS A NAMED CONSTANT, NOT A LITERAL, and build.py's link check is
// what established that.  Written `0.13f`, MSVC emits the COMDAT literal
// `__real@3e051eb8` -- the same bytes, and build.py reports
//
//      WOULD NOT LINK: 1 symbol(s) resolve to more than one address.
//        __real@3e051eb8
//            82003A78  referenced from 8219FCBC
//            8200DC5C  referenced from 822553EC
//
// because sub_822553D8 (src/z4_speed_check_b.cpp) loads the identical value
// from a DIFFERENT address.  A `__real@` COMDAT cannot resolve to two
// addresses: the linker folds duplicate definitions of one name to a single
// one, which is what it did for this project's other float literals
// (`__real@00000000` reaches 82002DA4 from two separate files, and
// `__real@3f800000` reaches 82002D40).  So these two words are not literals
// of the same name.
//
// The surrounding .rdata says the same thing from the other side.  82003A70
// onward reads
//
//      3efa35dd 3f000000 3e051eb8 40a00000 40000000 40a00000 40000000
//      0.488692 0.5      0.13     5.0      2.0      5.0      2.0
//
// -- 5.0 and 2.0 REPEATED ADJACENTLY inside one pool, which a COMDAT literal
// pool cannot contain, and 8200DC48 onward is a near-copy of the same run.
// These are elements of constant objects belonging to two sibling
// translation units, not a shared literal pool.  So the constant is declared
// here as an external named `const f32` at its address -- byte-identical to
// the literal, and unique per file.  The declaration claims a named constant
// at 82003A78 and nothing about how large the object containing it is.
//
// Three of the twelve words are relocated -- the lis/lfs pair addressing the
// constant, and the bl -- so 9 of 12 are compared.

struct SpeedDeep;

int SpeedBelow(SpeedDeep* d, f32 limit);

struct SpeedTop
{
    /* 0x00 */ char       unk0000[8];
    /* 0x08 */ SpeedDeep* deep;
};
ASSERT_OFFSET(SpeedTop, deep, 0x08);

extern const f32 kFloat_82003A78;   /* 3E051EB8 == 0.13f */

u8 IsSlow(SpeedTop* t)
{
    return SpeedBelow(t->deep, kFloat_82003A78);
}
