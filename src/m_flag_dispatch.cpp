#include "types.h"

// sub_8216CDA0 -- pass a bit through as a 0/1 argument. 40 B, 6 callers.
//
//      lhz     r10,38(r3)
//      mr      r11,r4          <- dead: r11 is never read again
//      mr      r3,r4
//      clrlwi  r9,r10,31       flags & 1
//      cmplwi  cr6,r9,0
//      beq-    cr6,zero
//      li      r4,1
//      b       0x82168EA8
// zero:li      r4,0
//      b       0x82168EA8
//
// TWO tail calls to the SAME address rather than one call with a computed
// argument. That is the source having written two calls, not the compiler
// expanding a ternary -- a ternary on a 0/1 value would have used the mask
// result directly and saved eight bytes.
//
// The `beq-` jumps away to the zero case, so the bit-set call is the
// fall-through and is written first.
struct Flagged
{
    char unk0000[38];
    u16  flags;
};
ASSERT_OFFSET(Flagged, flags, 38);

int Apply(void* target, int enable);

int ApplyFlag(const Flagged* f, void* target)
{
    if (f->flags & 1)
        return Apply(target, 1);
    return Apply(target, 0);
}
