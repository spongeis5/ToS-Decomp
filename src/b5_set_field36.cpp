#include "types.h"

// sub_8214C778 -- guarded store of one pointer field. 16 B, 5 callers.
//
//      cmplwi  cr6,r3,0
//      beqlr   cr6
//      stw     r4,36(r3)
//      blr
//
// `beqlr` is a guard written as a conditional RETURN, so the body is the
// fall-through and the positive path is written first (the branch-polarity
// rule). The immediate neighbour sub_8214C788 is the same shape writing two
// fields at 44 and 40.

struct Holder24
{
    /* 0x00 */ char  unk0000[0x24];
    /* 0x24 */ void* field24;
};
ASSERT_OFFSET(Holder24, field24, 0x24);

void SetField24(Holder24* h, void* v)
{
    if (h != 0)
        h->field24 = v;
}
