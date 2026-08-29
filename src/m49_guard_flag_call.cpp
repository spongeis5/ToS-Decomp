// sub_822D91C8 -- do nothing unless a byte flag is set. 20 bytes, 3 callers.
//
//      lbz     r11,49(r3)
//      cmplwi  cr6,r11,0
//      beqlr   cr6
//      b       0x822D90B0
//      blr                          <- unreachable, and counted in the size
//
// The pair with src/m47_guard_count_call.cpp, and the compare is the
// difference: `cmplwi` on an `lbz` is an unsigned byte, where m47's `cmpwi`
// on an `lwz` is a signed int. Same shape, two different field types, and the
// comparison names each one.
//
// `beqlr` is the conditional-RETURN idiom, so the guard is the early exit and
// the call is the fall-through.
//
// The tail branch is relocated, so 4 of 5 words are compared.

#include "types.h"

struct Dirty
{
    /* 0x00 */ u8 unk0000[49];
    /* 0x31 */ u8 dirty;
};

ASSERT_OFFSET(Dirty, dirty, 49);

void Rebuild(Dirty* d);

void RebuildIfDirty(Dirty* d)
{
    if (d->dirty != 0)
        Rebuild(d);
}
