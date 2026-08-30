#include "types.h"

// sub_821F0108 -- route on three flags to one of two tail calls, both on
// this->f56. 52 B, 4 callers.
//
//      lbz     r11,197(r3)  ; beq- -> the else block at the END
//      lbz     r11,198(r3)  ; beq- -> same
//      lwz     r11,204(r3)  ; cmpwi 1 ; beq- -> same
//      lwz     r3,56(r3)
//      b       8218C5C0     tail 1
//      lwz     r3,56(r3)
//      b       8218C658     tail 2
//
// All three guards branch FORWARD to one shared else block, which is the
// single `if (a && b && c) X; else Y;` shape: each condition branches away
// to the same block, and the interesting path is the fall-through.

struct Router
{
    /* 0x00 */ char  unk0000[56];
    /* 0x38 */ void* f56;
    /* 0x3C */ char  unk003C[137];
    /* 0xC5 */ u8    flagA;
    /* 0xC6 */ u8    flagB;
    /* 0xC7 */ char  unk00C7[5];
    /* 0xCC */ s32   mode;
};

ASSERT_OFFSET(Router, flagA, 197);
ASSERT_OFFSET(Router, flagB, 198);
ASSERT_OFFSET(Router, mode, 204);
ASSERT_OFFSET(Router, f56, 56);

void Tail1_8218C5C0(void*);
void Tail2_8218C658(void*);

void Route(Router* r)
{
    if (r->flagA && r->flagB && r->mode != 1)
        Tail1_8218C5C0(r->f56);
    else
        Tail2_8218C658(r->f56);
}
