// sub_822AC580 -- shift three arguments up one slot and put a global's
// address in front of them. 28 bytes, 3 callers.
//
//      mr      r11,r3
//      lis     r10,-32101
//      mr      r6,r5
//      mr      r5,r4
//      addi    r3,r10,9380         -> 829B24A4
//      mr      r4,r11
//      b       0x822A1E48
//
// The shuffle has to run from the TOP down -- r5 into r6 before r4 into r5 --
// or an argument would be overwritten before it is read, and r3 is parked in
// r11 first because the global's address needs that register. That ordering
// is forced by the register file, not chosen.
//
// `addi` off a relocated `lis` with no load: the first argument is the
// ADDRESS of a global object, not a pointer read out of one.
//
// The tail branch and the addi are relocated, so 5 of 7 words are compared.

#include "types.h"

struct Sink;
struct Payload;

extern Sink g_defaultSink;

void Deliver(Sink* s, Payload* p, int kind, int flags);

void DeliverDefault(Payload* p, int kind, int flags)
{
    Deliver(&g_defaultSink, p, kind, flags);
}
