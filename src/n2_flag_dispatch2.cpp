// sub_82252EF8 -- follow a pointer field, and if it is there hand it to one
// of two routines depending on a byte flag. 36 B, 4 callers.
//
//      lwz     r3,4(r3)         the pointer, straight into the argument reg
//      cmplwi  cr6,r3,0
//      beqlr   cr6              guard written as a conditional RETURN
//      clrlwi  r11,r4,24        the flag is BYTE-wide
//      cmplwi  cr6,r11,0        UNSIGNED
//      bne-    cr6,other
//      b       0x82151328       flag == 0  -- the FALL-THROUGH arm
// other:
//      b       0x821716D0       flag != 0
//      blr                      unreachable, appended after a tail call
//
// BRANCH POLARITY IS SOURCE ORDER. The branch taken is `bne-`, i.e. taken
// when the flag is NON-zero, and the fall-through arm is the zero one. MSVC
// branches to the ELSE block when the tested condition is false, so the
// condition in the source is `flag == 0` and its arm is written FIRST.
// Written the other way round -- `if (flag) Other(); else Plain();` -- the
// test comes out `beq-` and the two `b`s swap.
//
// `clrlwi r11,r4,24` here is on the ARGUMENT, not on the result, so it is
// not the bool-return signature from MATCHED.md; it is a byte-wide parameter
// being widened for the compare. `bool` and `u8` are indistinguishable in
// that position.
//
// Both arms are `b`, not `bl`, so this returns whatever they do and nothing
// of its own -- consistent with `void`, since the `beqlr` path returns no
// value either.
//
// Both `b`s are relocated; the other 7 words are compared.

#include "types.h"

struct FlagHolder
{
    /* 0x00 */ char  unk0000[0x04];
    /* 0x04 */ void* target;
};
ASSERT_OFFSET(FlagHolder, target, 0x04);

void HandlePlain(void* t);
// Named for its ADDRESS, not by analogy. `HandleOther` was
// already taken by owner_clear.cpp for 826E50D0, a different
// function; one name resolving to two addresses verifies byte
// for byte and could never link, which is what build.py's
// WOULD NOT LINK check exists to catch.
void HandleFlagged_821716D0(void* t);

void DispatchByFlag(FlagHolder* h, bool flag)
{
    void* t = h->target;
    if (t == 0)
        return;

    if (flag == 0)
        HandlePlain(t);
    else
        HandleFlagged_821716D0(t);
}
