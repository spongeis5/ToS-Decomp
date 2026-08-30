#include "types.h"

// sub_825E45E8 -- attach or detach through a virtual, and only record the
// new state when the virtual succeeded. 136 B.  Bridge between Acc_825E45E0
// and TypeId_825E4670.
//
//      cmplwi cr6,r4,0 ; beq- else            unsigned test -> a pointer
//      lbz    r11,8(r3) ; cmplwi ; bne- zero  already attached -> 0
//      lwz    r11,0(r3) ; lwz r11,36(r11) ; bctrl    slot 9, r4 untouched
//      cmpwi  r3,0 ; bne- out                 the error is returned as is
//      li     r11,1 ; b store
// else: lwz   r11,0(r31) ; li r4,0 ; mr r3,r31
//       lwz   r11,36(r11) ; bctrl
//       cmpwi r3,0 ; bne- out
//       li    r11,0
// store:stb   r11,8(r31)
// zero: li    r3,0
//
// The two arms' identical trailing store is tail-merged into one `stb`, and
// the guard's `return 0` shares the same `li r3,0`, so the two branches are
// written out separately and MSVC joined them.

struct Obj;

struct ObjVT
{
    void* slot[9];
    s32 (*attach)(Obj*, void*);
};

struct Obj
{
    /* 0x00 */ ObjVT* vt;
    /* 0x04 */ u8     unk0004[4];
    /* 0x08 */ u8     attached;
};
ASSERT_OFFSET(Obj, attached, 0x08);

s32 SetAttached(Obj* o, void* p)
{
    s32 r;

    if (p != 0)
    {
        if (o->attached != 0)
            return 0;
        r = o->vt->attach(o, p);
        if (r != 0)
            return r;
        o->attached = 1;
    }
    else
    {
        r = o->vt->attach(o, 0);
        if (r != 0)
            return r;
        o->attached = 0;
    }
    return 0;
}
