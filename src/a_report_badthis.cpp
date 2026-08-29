// sub_82806D08 -- 20 bytes, 132 callers. The most-called unattributed leaf
// left in the image, and it is an error reporter.
//
//      lis     r11,-32247
//      lwz     r3,24(r3)
//      mr      r5,r4
//      addi    r4,r11,-21336        ; = 8208ACA8
//      b       0x827c8678
//
// 8208ACA8 is the ASCII
//      "Error: Null or invalid id 'this' is used for a method of %s class.\n"
// and 827C8678 has the classic varargs prologue (std r5..r10 into the
// caller's parameter save area) so it is the engine's formatted printer.
//
// Register discipline: the incoming r4 is moved UP to r5 before r4 is
// overwritten with the format string, which is exactly `Print(x, fmt, arg)`
// -- the second source argument becomes the third machine argument because
// the format sits between them.
//
// 3 of the 5 words are relocated (the lis/addi pair for the string and the
// tail branch), so 2 are actually compared.

#include "types.h"

struct Reporter
{
    /* 0x00 */ char  unk0000[0x18];
    /* 0x18 */ void* sink;
};

ASSERT_OFFSET(Reporter, sink, 0x18);

void Print(void* sink, const char* fmt, ...);

void ReportBadThis(Reporter* r, const char* cls)
{
    Print(r->sink,
          "Error: Null or invalid id 'this' is used for a method of %s class.\n",
          cls);
}
