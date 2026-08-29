#include "types.h"

// sub_8279D958 -- virtual call on a member object. 20 B, 8 callers.
//
//      lwz     r3,8(r3)
//      lwz     r11,0(r3)
//      lwz     r10,16(r11)
//      mtctr   r10
//      bctr
//
// Slot 16/4 = 4. The row is recorded as 44 bytes and holds THREE bodies:
// this one, then `stb r4,12(r3) ; blr` at 8279D970 after a zero word, then
// `lbz r11,12(r4) ; stb r11,0(r3) ; blr` at 8279D978. can_shrink() proves
// the row covers more than one and compares only this.
struct Part;

struct PartVT
{
    void* slot0;
    void* slot1;
    void* slot2;
    void* slot3;
    int (*Run)(Part*);
};
ASSERT_OFFSET(PartVT, Run, 16);

struct Part
{
    const PartVT* vt;
};

struct Holder8
{
    char  unk0000[8];
    Part* part;
};
ASSERT_OFFSET(Holder8, part, 8);

int RunPart(Holder8* h)
{
    Part* p = h->part;
    return p->vt->Run(p);
}
