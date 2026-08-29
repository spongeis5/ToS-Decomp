// sub_827882D0 -- forward six arguments to an eight-argument function,
// filling the two new slots with zero. 16 bytes, 3 callers.
//
//      mr      r10,r8              the LAST incoming argument moves up two
//      li      r9,0
//      li      r8,0
//      b       0x82786680
//
// The `mr` has to come first: r8 is both the source of the eighth argument
// and the destination of the sixth, so reading it before clobbering it is
// forced, and that is why the two `li` are in descending register order.
//
// sub_82786680 really does take eight GPR arguments and no float ones -- it
// spills r3..r9 to its frame on entry and null-tests r10 to substitute a
// default, so the two zeros are ordinary arguments and not padding.
//
// The tail branch is relocated, so 3 of 4 words are compared.

#include "types.h"

struct Ctx78;
struct Doc78;

void EmitFull(Ctx78* c, Doc78* d, int x, int y, u8 flag,
              void* a, void* b, void* opt);

void EmitDefault(Ctx78* c, Doc78* d, int x, int y, u8 flag, void* opt)
{
    EmitFull(c, d, x, y, flag, 0, 0, opt);
}
