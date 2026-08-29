// sub_8253FD70 -- guarded accumulate into an array slot, 28 bytes, 82 callers.
//
//      cmplwi  cr6,r3,0
//      beqlr   cr6                 return if the pointer is null
//      rlwinm  r11,r4,2,0,29       r11 = i * 4
//      lwzx    r10,r11,r3
//      add     r10,r10,r5
//      stwx    r10,r11,r3
//      blr
//
// The guard is a conditional RETURN (beqlr), not a branch over the body, so
// the null case falls out with nothing to skip -- the same shape 82806FD0
// wanted and did not get. Here there is no default return value to
// materialise, which is what made that one awkward.
//
// rlwinm r11,r4,2,0,29 is (r4 << 2) & ~3 -- plain int indexing.

void ArrayAdd(int* a, int i, int v)
{
    if (a)
        a[i] += v;
}
