// sub_82807B50 -- run the handler for every kind except four of them.
// 44 B, 3 callers.
//
//      lbz    r11,0(r3)
//      cmplwi cr6,r11,8 ; beqlr cr6
//      cmplwi cr6,r11,5 ; bltlr cr6
//      cmplwi cr6,r11,6 ; beqlr cr6
//      cmplwi cr6,r11,9 ; beqlr cr6
//      b      0x828071C8
//      (blr)                       unreachable
//
// Four tests, each with its OWN conditional return and no shared exit block,
// and the call as the fall-through: that is one `||` guard written before
// the body, with the terms in source order.  A switch or a chain of `if`
// bodies would need somewhere to jump to.
//
// `cmplwi` throughout, including the `bltlr`, so the kind is unsigned -- the
// `< 5` test is an unsigned one and cannot be a negative check.
//
// The order 8, 5, 6, 9 is not sorted and is not a range test the compiler
// built; it is the order the terms were written in.

#include "types.h"

struct Kinded;

void RunHandler(Kinded* k);

struct Kinded
{
    /* 0x00 */ u8 kind;
};
ASSERT_OFFSET(Kinded, kind, 0x00);

void RunIfHandled(Kinded* k)
{
    u8 kind = k->kind;

    if (kind == 8 || kind < 5 || kind == 6 || kind == 9)
        return;

    RunHandler(k);
}
