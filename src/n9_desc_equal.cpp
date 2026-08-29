// sub_826B2618 -- compare two descriptors field by field. 144 B, 4 callers.
//
//      lwz   r11,0(r3)  ; lwz  r10,0(r4)  ; cmplw cr6,r11,r10 ; bne- fail
//      lhz   r11,4(r4)  ; lhz  r10,4(r3)  ; cmplw cr6,r10,r11 ; bne- fail
//      lbz   r11,6(r4)  ; lbz  r10,6(r3)  ; cmplw cr6,r10,r11 ; bne- fail
//      ... the same three words for +7, +8, +9, +10
//      lbz   r11,11(r4) ; lbz  r10,11(r3) ; cmplw cr6,r10,r11
//      li    r11,1
//      beq-  cr6,out
// fail:li    r11,0
// out: clrlwi r3,r11,24
//      blr
//
// EIGHT GUARDS, ALL BRANCHING FORWARD TO ONE SHARED `li r11,0`, which is the
// short-circuit form: one `&&` expression, not eight `if`s. MATCHED.md's
// sub_821675B8 is the control -- three flat `if (x) return 0;` guards make
// MSVC plant the failure value after the FIRST test and branch backward into
// it, displacing twelve words. Here the failure value is written once and
// last, so it is the tail of a single return expression.
//
// The trailing `clrlwi r3,r11,24` is `bool` (MATCHED.md): u8, char and int
// returns compute the 0/1 straight into r3 with no mask.
//
// Every compare is `cmplw`, UNSIGNED, on all eight fields -- so nothing here
// is signed. The widths come from the loads: `lwz` at +0, `lhz` at +4, and
// six `lbz` at +6 through +11, which accounts for all twelve bytes with no
// hole.
//
// The compares all read r3's field in rA and r4's in rB, so the left operand
// of each `==` is the first parameter's. The LOAD order flips after the first
// pair -- +0 loads r3 then r4, the other seven load r4 then r3 -- which is
// scheduling, not source: the operand order in the compare is what carries
// the information.
//
// Nothing is relocated; all 36 words are compared.

#include "types.h"

struct Desc
{
    /* 0x00 */ u32 id;
    /* 0x04 */ u16 f04;
    /* 0x06 */ u8  f06;
    /* 0x07 */ u8  f07;
    /* 0x08 */ u8  f08;
    /* 0x09 */ u8  f09;
    /* 0x0A */ u8  f0A;
    /* 0x0B */ u8  f0B;
};
ASSERT_OFFSET(Desc, f04, 0x04);
ASSERT_OFFSET(Desc, f0B, 0x0B);

bool DescEqual(const Desc* a, const Desc* b)
{
    return a->id  == b->id
        && a->f04 == b->f04
        && a->f06 == b->f06
        && a->f07 == b->f07
        && a->f08 == b->f08
        && a->f09 == b->f09
        && a->f0A == b->f0A
        && a->f0B == b->f0B;
}
