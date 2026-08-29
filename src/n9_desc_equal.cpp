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
// IT IS A MEMBER FUNCTION, and that is the whole of what was hard here.
// Written as a free function taking two pointers, this scores 15 of 36 with
// EVERY BRANCH, the tail and the whole first comparison already correct --
// the 21 wrong words are the seven narrow comparisons, and in each one the
// two loads are issued in the opposite order:
//
//      want  lbz r11,6(r4) ; lbz r10,6(r3) ; cmplw cr6,r10,r11
//      free  lbz r11,6(r3) ; lbz r10,6(r4) ; cmplw cr6,r11,r10
//
// Both compare the SAME operands in the same rA/rB roles -- r3's field is rA
// in both -- so the `==` operand order was already right and reversing it
// makes things worse, not better (20 of 36 reversed everywhere, 22 of 36
// reversed only on the narrow terms). What differs is which side's load is
// issued first, and only for the sub-word fields; the `lwz` pair at +0 is
// left-then-right in both.
//
// `bool Desc::Eq(const Desc& o) const` is 36 of 36, at BOTH optimisation
// levels. So this is MATCHED.md's member-function lever showing up on a new
// symptom: there it was registers coming out transposed, here it is load
// ISSUE ORDER, and in both cases `this` is not simply the first parameter as
// far as scheduling is concerned. Fourteen shapes were measured; `const&`
// parameters (15), non-const pointers (15), a nested sub-struct (15), a named
// `const Desc&` for the second operand (15) and an inlined `EqB(x,y)` helper
// (15, or 21 with its arguments swapped) all leave it exactly where the free
// function was.
//
// WHAT THE BYTES DO NOT DECIDE is which member: `Eq(const Desc&)`,
// `Eq(const Desc*)` and `operator==(const Desc&)` are byte-identical at
// 36 of 36. The named form is written because it claims least.
//
// Reading `o.id == id` instead of `id == o.id` is 27 of 36, so the operand
// order IS readable here even though the member form is what unlocks it.
//
// EIGHT GUARDS, ALL BRANCHING FORWARD TO ONE SHARED `li r11,0`, which is the
// short-circuit form: one `&&` expression, not eight `if`s. MATCHED.md's
// sub_821675B8 is the control -- three flat `if (x) return 0;` guards make
// MSVC plant the failure value after the FIRST test and branch backward into
// it, displacing twelve words.
//
// The trailing `clrlwi r3,r11,24` is `bool`, and here that is MEASURED rather
// than inferred: the identical body returning `int` is 140 bytes, one
// instruction shorter, with no mask -- exactly what MATCHED.md predicts.
//
// Every compare is `cmplw`, UNSIGNED, on all eight fields. The widths come
// from the loads: `lwz` at +0, `lhz` at +4, six `lbz` at +6..+11, which
// accounts for all twelve bytes with no hole.
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

    bool Eq(const Desc& o) const;
};
ASSERT_OFFSET(Desc, f04, 0x04);
ASSERT_OFFSET(Desc, f0B, 0x0B);

bool Desc::Eq(const Desc& o) const
{
    return id  == o.id
        && f04 == o.f04
        && f06 == o.f06
        && f07 == o.f07
        && f08 == o.f08
        && f09 == o.f09
        && f0A == o.f0A
        && f0B == o.f0B;
}
