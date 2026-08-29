#include "types.h"

// sub_82799878 -- store a value and its quarter, squared. 28 B, 12 callers.
//
//      lis     r11,-32256
//      stfs    f1,0(r3)
//      lfs     f0,12236(r11)   ; = 82002FCC, the pool entry 0.25f
//      fmuls   f0,f1,f0
//      fmuls   f0,f0,f0
//      stfs    f0,4(r3)
//      blr
//
// Two multiplies and no fmadds, so nothing to fuse; the only thing to get
// right is that the SECOND multiply squares the first result rather than
// multiplying the input again. `fmuls f0,f0,f0` says so unambiguously.
//
// NEEDS /O2 /Os. At plain /O2 the square goes to a FRESH register --
// `fmuls f13,f0,f0` and `stfs f13,4(r3)` -- where the target updates f0 in
// place. That is a register choice, not a source one: mutating the local
// (`h = h * h;`) compiles to exactly the same thing at /O2, so there was
// never a source shape to find. The level was.
struct RadiusPair
{
    f32 value;
    f32 halfSquared;
};
ASSERT_OFFSET(RadiusPair, halfSquared, 4);

void SetRadius(RadiusPair* p, f32 v)
{
    p->value = v;
    f32 h = v * 0.25f;
    p->halfSquared = h * h;
}
