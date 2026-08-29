// sub_82665388 -- recorded as 16 bytes / 25 callers, but the inventory has
// MERGED TWO FUNCTIONS. The image holds
//
//  82665388  lwz r3,24(r3)  /  b 0x826652E8      <- 8 bytes
//  82665390  lwz r3,24(r3)  /  b 0x82665330      <- 8 bytes, a second body
//
// with no padding between them; sub_826653B0 twenty-four bytes later is the
// same 8-byte shape (`lwz r3,4(r3) ; b 0x82665330`) and IS recorded on its
// own. So 16 is two adjacent 8-byte leaves, not one function, and no single
// source symbol can reproduce it -- match.py splits a COFF section at symbol
// boundaries, so two functions come back as two entries whichever way the
// object is compiled.
//
// This file forwards the object's member at +0x18 as the only argument of a
// tail call. MEASURED against both halves of the row: 2 words each,
// 1 identical and 1 relocated (the tail branch) for the body at 82665388 AND
// for the body at 82665390. One source reproduces both, which is the proof
// that the row is two copies of this shape with different callees, not one
// 16-byte function.
//
// It therefore cannot report MATCH: match.py requires len(code) == recorded
// size, and 8 != 16.

#include "types.h"

struct Owner
{
    /* 0x00 */ char  unk0000[0x18];
    /* 0x18 */ void* member;
};

ASSERT_OFFSET(Owner, member, 0x18);

int Query(void* p);

int QueryMember(Owner* o)
{
    return Query(o->member);
}
