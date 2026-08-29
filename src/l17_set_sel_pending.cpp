// sub_82249E18 -- set a selector byte, remembering a pending one. 92 B,
// 4 callers.
//
//      clrlwi r11,r4,24 ; cmplwi ; beq- body
//      clrlwi r10,r5,24 ; cmplwi ; beq- body
//      lbz    r10,125(r3) ; cmplwi ; beqlr
// body:li     r10,0
//      cmplwi cr6,r11,0
//      stb    r10,125(r3)
//      bne-   cr6,tail
//      lbz    r10,79(r3) ; cmplwi ; beq- cr6,tail
//      stb    r5,125(r3)
// tail:lbz    r10,79(r3)
//      cmplw  cr6,r10,r11
//      beqlr  cr6
//      stb    r4,79(r3)
//      b      0x822493D8
//
// Three readings that the register discipline settles:
//
//  * the first three tests are ONE short-circuit guard, not three statements:
//    the two `beq-` exits jump FORWARD past a `beqlr` into the body, which is
//    what `if (a && b && p->pending == 0) return;` gives -- each false term
//    skips to the body and only the last test returns.
//  * the second `lbz r10,79(r3)` is a genuine reload, not a missed CSE: a
//    store to 125 sits between the two reads on one path, and the tail is
//    reached from both.
//  * the stores use the RAW parameter registers (`stb r5`, `stb r4`) while
//    the tests use the masked copies, which is what byte-wide parameters do:
//    a store truncates anyway, a comparison does not.
//
// The tail call keeps r3, so the notify takes the same object.

#include "types.h"

struct Selector;

void NotifySelectorChanged(Selector* s);

struct Selector
{
    /* 0x00 */ char unk0000[0x4F];
    /* 0x4F */ u8   sel;
    /* 0x50 */ char unk0050[0x2D];
    /* 0x7D */ u8   pending;
};
ASSERT_OFFSET(Selector, sel,     0x4F);
ASSERT_OFFSET(Selector, pending, 0x7D);

void SetSelector(Selector* s, u8 want, u8 pend)
{
    if (want != 0 && pend != 0 && s->pending == 0)
        return;

    s->pending = 0;
    if (want == 0 && s->sel != 0)
        s->pending = pend;

    if (s->sel != want)
    {
        s->sel = want;
        NotifySelectorChanged(s);
    }
}
