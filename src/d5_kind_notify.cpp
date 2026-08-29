// sub_8225F168 -- tail-call a notifier with two zero arguments for five
// particular kind values. 56 B, 5 callers.
//
//      cmpwi   cr6,r3,19 ; beq- cr6,0x8225F190
//      cmpwi   cr6,r3,22 ; beq- cr6,0x8225F190
//      cmpwi   cr6,r3,20 ; beq- cr6,0x8225F190
//      cmpwi   cr6,r3,23 ; beq- cr6,0x8225F190
//      cmpwi   cr6,r3,24 ; bnelr cr6
//  8225F190:
//      li      r4,0
//      li      r3,0
//      b       0x82614868
//      blr                        unreachable, appended after the tail call
//
// `cmpwi` is a SIGNED compare, so the parameter is `int` and not an unsigned
// or a byte.
//
// The compare ORDER is 19, 22, 20, 23, 24 -- not sorted, and not a range
// test. A `switch` over five values spanning 19..24 would get a table or a
// bit test, and MSVC lays switch bodies out in source order rather than
// re-ordering the tests; a `||` chain tests in source order, which is what
// this is. So the sequence above IS the source order and is written out
// unsorted on purpose.
//
// Every arm branches FORWARD to one shared call, and the last arm is a
// conditional RETURN (`bnelr`) rather than a branch, which is the shape of a
// single `if` with the whole chain in it.
//
// One word is relocated (the `b` to 82614868), so 13 of 14 are compared.
// The trailing `blr` is the unreachable one MSVC appends after a tail call.

#include "types.h"

void NotifyKind(int a, int b);

void NotifyIfKind(int kind)
{
    if (kind == 19 || kind == 22 || kind == 20 || kind == 23 || kind == 24)
        NotifyKind(0, 0);
}
