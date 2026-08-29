#include "types.h"

// sub_827727D0 -- advance a 16-bit character cursor by one token. 84 B,
// 12 callers.
//
//      lwz     r11,4(r3)        p = s->cur
//      lbz     r10,16(r3)       s->flag
//      cmplwi  r10,0
//      stw     r11,0(r3)        s->start = p
//      beq-    no
//      lhz     r10,0(r11)
//      cmplwi  cr6,r10,38
//      li      r10,1
//      beq-    cr6,yes
// no:  li      r10,0
// yes: clrlwi. r10,r10,24       <- a bool, materialised then tested
//      beq-    plain
//      b       0x82771A38       tail call
// plain:lwz    r10,8(r3)
//      cmplw   cr6,r11,r10
//      bgelr   cr6              p >= s->end: nothing to do
//      lhz     r10,0(r11)
//      addi    r11,r11,2
//      stw     r11,4(r3)        s->cur = p + 1
//      stw     r10,12(r3)       s->tok = *p
//      blr
//
// The `&&` is materialised into a byte and only then tested, the same tell as
// sub_8219FCD8: a bare `if (a && b)` branches out of each term without ever
// building a value. So the condition is an inlined bool-returning helper.
//
// Store order is source order: `cur` at 4 goes out before `tok` at 12, so the
// pointer bump is written first even though it reads oddly. The `lbz` at 16
// is hoisted ABOVE the store to offset 0 -- MSVC schedules across stores to
// distinct offsets of the same object.
//
// NAMING THE CHARACTER IN A LOCAL IS WHAT ORDERS THE TAIL. Written as
//
//     s->cur = p + 1;
//     s->tok = *p;
//
// the load sinks BELOW the pointer bump and its store -- addi, stw 4, lhz,
// stw 12 -- because nothing forces it earlier and the store to offset 4
// cannot alias the u16 it is about to read. Reading it into a local first
// pins it ahead of both: lhz, addi, stw 4, stw 12, which is the target. Four
// words, and it also settles which of r10/r11 holds which value.
//
// `bgelr` is a guard written as a conditional return, so the body is the
// fall-through and the guard has to come first.
//
// NEEDS /O2 /Os: `clrlwi.` in one instruction instead of `clrlwi` plus a
// separate `cmplwi cr6`, which is the same one-word /Os property that decided
// sub_827156B8.
struct Scanner
{
    /* 0x00 */ const u16* start;
    /* 0x04 */ const u16* cur;
    /* 0x08 */ const u16* end;
    /* 0x0C */ u32        tok;
    /* 0x10 */ u8         flag;
};
ASSERT_OFFSET(Scanner, cur,  0x04);
ASSERT_OFFSET(Scanner, end,  0x08);
ASSERT_OFFSET(Scanner, tok,  0x0C);
ASSERT_OFFSET(Scanner, flag, 0x10);

void ScanSpecial(Scanner* s);

static bool IsSpecial(const Scanner* s, const u16* p)
{
    return s->flag != 0 && *p == 38;
}

void ScanNext(Scanner* s)
{
    const u16* p = s->cur;
    u16 c;

    s->start = p;

    if (IsSpecial(s, p))
    {
        ScanSpecial(s);
        return;
    }

    if (p >= s->end)
        return;

    c = *p;
    s->cur = p + 1;
    s->tok = c;
}
