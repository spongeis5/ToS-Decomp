#include "types.h"

// sub_82696A60 -- name the argument object "Child", hand it this object's
// context, then register it. 108 B.  Bridge between Acc_82696A48 and
// Acc_82696AD0.
//
//      lwz  r11,0(r4) ; mr r31,r3 ; mr r30,r4 ; mr r3,r4
//      addi r4,r10,-14944            -> 8206C5A0 "Child"
//      lwz  r9,12(r11)               vtable slot 3
//      li   r6,0 ; lwz r5,24(r31) ; bctrl
//      mr   r4,r30 ; addi r5,r8,-25920   -> 82A49AC0
//      mr   r3,r31 ; bl 0x8262fd90
//
// The second call's result is returned without being touched, and 8262FD90
// ends in a tail branch to the restore thunk, so both are void.

struct Obj;

struct ObjVT
{
    void* slot0;
    void* slot1;
    void* slot2;
    void (*name)(Obj*, const char*, void*, s32);
};

struct Obj
{
    /* 0x00 */ ObjVT* vt;
};
ASSERT_OFFSET(Obj, vt, 0x00);

struct Holder
{
    /* 0x00 */ u8    unk0000[0x18];
    /* 0x18 */ void* ctx;
};
ASSERT_OFFSET(Holder, ctx, 0x18);

struct Registry;
extern Registry g_childRegistry;

extern void RegisterChild(Holder* h, Obj* o, Registry* r);

void BindChild(Holder* h, Obj* o)
{
    o->vt->name(o, "Child", h->ctx, 0);
    RegisterChild(h, o, &g_childRegistry);
}
