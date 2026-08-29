// sub_8223C1F0 -- pick one of two calls on the same child, according to two
// byte flags. 40 bytes, 3 callers.
//
//      lbz    r11,162(r3) ; cmplwi cr6,r11,0 ; beq- cr6,<else>
//      lbz    r11,161(r3) ; cmplwi cr6,r11,0 ; beq- cr6,<else>
//      lwz    r3,56(r3)   ; b 0x8218C5C0
//  else:
//      lwz    r3,56(r3)   ; b 0x8218C658
//
// Both tests branch to the SAME block, which is the short-circuit `&&`: each
// failing term jumps to the else arm and the true path falls through. Two
// separate `if`s would have given the first one its own body.
//
// The child load is DUPLICATED, once per arm, and that is not a missed CSE --
// each arm ends in a tail call, so there is no join for a shared load to live
// in.
//
// `lbz` plus `cmplwi` on both: unsigned bytes. The flag at 162 is tested
// first even though it is the higher offset, and store-order reasoning does
// not apply to loads, so that order is the source's.
//
// Both tail branches are relocated, so 8 of 10 words are compared.

#include "types.h"

struct Child;

struct Owner3C
{
    /* 0x00 */ u8     unk0000[0x38];
    /* 0x38 */ Child* child;
    /* 0x3C */ u8     unk003C[0x65];
    /* 0xA1 */ u8     ready;
    /* 0xA2 */ u8     enabled;
};

ASSERT_OFFSET(Owner3C, child, 0x38);
ASSERT_OFFSET(Owner3C, ready, 161);
ASSERT_OFFSET(Owner3C, enabled, 162);

void DrawActive(Child* c);
void DrawIdle(Child* c);

void DrawChild(Owner3C* o)
{
    if (o->enabled && o->ready)
        DrawActive(o->child);
    else
        DrawIdle(o->child);
}
