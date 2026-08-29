#include "types.h"

// sub_826C1260 -- walk a chain to its last link and report whether that link's
// counter is non-zero. 48 B of the 56-byte inventory row; 826C1290 is a
// separate two-instruction accessor sharing the row, so match.py shrinks.
// 6 callers.
//
//      lwz     r11,4(r4) ; cmpwi cr6,r11,0 ; beq- -> out
//   top:
//      mr      r4,r11 ; lwz r11,4(r11) ; cmpwi cr6,r11,0 ; bne+ top
//   out:
//      lwz     r11,12(r4) ; addic r10,r11,-1 ; subfe r9,r10,r11
//      stb     r9,0(r3) ; blr
//
// `addic rD,rS,-1 ; subfe rT,rD,rS` is branchless `x != 0`: no carry out when
// rS is zero, carry otherwise.
//
// The chain test is `cmpwi` -- SIGNED -- on a value that is then dereferenced.
// Every pointer null test in this image is `cmplwi`, so the link field is an
// `int` holding a pointer rather than a pointer (the sub_82631D98 rule), and
// the walk needs the cast.
//
// The first load is peeled out in front of the loop with the second copy at
// the bottom, which is the rotated `while`, not a `do/while`.

struct Link
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ s32  next;
    /* 0x08 */ char unk0008[0x04];
    /* 0x0C */ s32  count;
};
ASSERT_OFFSET(Link, next,  0x04);
ASSERT_OFFSET(Link, count, 0x0C);

void LastLinkHasCount(bool* out, Link* l)
{
    while (l->next != 0)
        l = (Link*)l->next;

    *out = (l->count != 0);
}
