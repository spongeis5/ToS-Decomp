#include "types.h"

// sub_82676F40 -- a 4-byte body: blr.  The 16-byte inventory row covers
// this empty function, a pad word, and an 8-byte accessor.  can_shrink
// must reconcile the row down to our 4 bytes; whether it accepts a zero
// PAD word at the cut is exactly what this attempt measures.

void NoOp()
{
}
