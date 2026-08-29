#include "types.h"

// sub_826C1470 -- address of a sub-object at +40. Recorded as 16 bytes, but
// the row holds TWO identical 8-byte bodies:
//
//   826C1470  addi r3,r3,40 ; blr
//   826C1478  addi r3,r3,40 ; blr
//
// so one source symbol is 8 bytes and match.py shrinks the window. 5 callers.
//
// There is no null test, so this is NOT a base-class upcast (those keep null
// null with `cmplwi`/`bne-`/`li 0`); it is a plain address-of.

struct Inner40
{
    /* 0x00 */ char unk0000[0x04];
};

struct Outer40
{
    /* 0x00 */ char    unk0000[0x28];
    /* 0x28 */ Inner40 inner;
};
ASSERT_OFFSET(Outer40, inner, 0x28);

Inner40* GetInner40(Outer40* o)
{
    return &o->inner;
}
