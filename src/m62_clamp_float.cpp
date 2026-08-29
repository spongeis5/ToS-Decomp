// sub_8215A9F8 -- clamp a float between two bounds. 44 bytes, 3 callers.
//
//      fmr    f0,f1
//      fcmpu  cr6,f1,f2
//      bgt-   cr6,<upper>
//      fmr    f1,f2 ; blr          the low bound
//  upper:
//      fcmpu  cr6,f0,f3
//      blt-   cr6,<pass>
//      fmr    f1,f3 ; blr          the high bound
//  pass:
//      fmr    f1,f0 ; blr          the value itself
//
// Three float arguments in f1, f2, f3 and no GPR touched, so the signature is
// three floats and the result comes back in f1.
//
// The branch polarities read straight off: `bgt-` skipping the low-bound
// return means the FALL-THROUGH is `x <= lo`, and `blt-` skipping the
// high-bound return means the fall-through is `x >= hi`. Both guards are
// written as early returns with the bound, and the value itself is the tail.
//
// `fmr f0,f1` at the top with `fmr f1,f0` at the bottom is the copy MSVC
// needs because f1 is both the incoming value and the return register: the
// two bound-returning arms overwrite f1, so the value has to live somewhere
// that survives them. The SECOND comparison then reads f0 rather than f1,
// which is the copy already in use rather than a second read of the argument.
//
// Nothing is relocated: 11 of 11 words are compared.

#include "types.h"

float ClampF(float x, float lo, float hi)
{
    if (x <= lo)
        return lo;

    if (x >= hi)
        return hi;

    return x;
}
