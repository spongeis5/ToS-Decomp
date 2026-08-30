#include "types.h"

// sub_8276D308 -- guarded accessor with an optional out-pointer.
// 48 B, 3 callers.
//
//      lwz     r11,16(r3)
//      cmplwi  cr6,r11,0
//      beq-    cr6,0x8276d330   -> li r3,0 ; blr   (the shared early exit)
//      cmplwi  cr6,r4,0
//      beq-    cr6,0x8276d324   -> skip the store
//      lwz     r11,0(r11)
//      stw     r11,0(r4)
//      lwz     r11,16(r3)       <- RELOADED, and the reason is visible:
//      addi    r3,r11,4            the store through `out` might alias
//                                  s->list, so MSVC must re-read it
//      blr
//      li      r3,0 ; blr
//
// The reload is aliasing, not the CSE-defeat lever: there IS a store between
// the two reads, through a pointer the compiler cannot prove distinct.

struct Entry
{
    /* 0x00 */ void* head;
    /* 0x04 */ char  unk0004[4];
};

ASSERT_OFFSET(Entry, head, 0);

struct Reg
{
    /* 0x10 */ char   unk0000[16];
    /* 0x10 */ Entry* list;
};

ASSERT_OFFSET(Reg, list, 16);

int* Get(Reg* r, void** out)
{
    if (r->list)
    {
        if (out != 0)
            *out = r->list->head;
        return (int*)&r->list->unk0004;
    }
    return 0;
}
