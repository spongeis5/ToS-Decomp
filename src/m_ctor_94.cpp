#include "types.h"

// sub_825FE880 -- constructor. 48 B, 25 callers.
//
//      lis     r10,-32249
//      li      r11,0
//      addi    r9,r10,-20664    ; = 8206AF48, the vtable
//      li      r8,1
//      stw     r9,0(r3)         VTABLE FIRST
//      stw     r11,8(r3)
//      stw     r11,12(r3)
//      stw     r11,16(r3)
//      stw     r11,20(r3)
//      stb     r8,148(r3)       +0x94, between +0x14 and +0x18
//      stw     r11,24(r3)
//      blr
//
// WRITTEN AS A REAL C++ CONSTRUCTOR, and that is why the vtable store is
// first. As a free function assigning `o->vt` explicitly, MSVC schedules
// that store AFTER the +8 store no matter which order the source uses --
// tried both ways, byte-identical output, because the store depends on a
// lis/addi still in flight and the scheduler fills the gap with an
// independent one.
//
// A compiler-emitted vptr store is not subject to that: it is planted at the
// top of the constructor before any user code exists to reorder with. So
// WHEN A STORE OF A VTABLE ADDRESS COMES FIRST AND WILL NOT MOVE, THE SOURCE
// IS A CONSTRUCTOR, not an initialise-this-struct function.
//
// MATCHED at /O2, and the last two words came out with THE ADDRESS-OF-MEMBER
// LEVER applied to the FIRST member store.
//
// What was wrong was the first two stores, transposed and nothing else: we
// emitted `stw r11,8(r3)` where the target emits `stw r9,0(r3)`, and vice
// versa. The four setup instructions ahead of them (lis, li r11,0, addi r9,
// li r8,1) are identical in both, so the vtable address is ready at exactly
// the same cycle either way -- it was a pure TIE-BREAK between two ready
// stores, and the target issues the one whose value was defined LATER.
//
// Writing the first member store through a pointer to the member
//
//      s32* p = &f08;
//      *p = 0;
//
// removes MSVC's proof that +0 and +8 cannot alias, so it can no longer
// reorder the two, and the compiler-emitted vptr store -- which is planted
// first, before any user code -- stays first. 10 of 10.
//
// The variants say the pin has to be on the member that MOVED: `&f08` is 10
// of 10, an `s32&` reference to it is 10 of 10, and routing all five zeroes
// through one `s32* p` is 10 of 10, while pinning `&enabled` instead leaves
// the baseline's 8 of 10 untouched. As on sub_827007F8 an inlined
// `SetI(s32*, s32)` helper does NOT work -- 8 of 10, byte-identical to the
// baseline -- so the bare local pointer and the helper form of this lever
// are not interchangeable.
//
// A REAL BASE CLASS IS THE WRONG ANSWER here, and it is worth saying so
// because it is the first thing the constructor levers suggest: moving the
// virtual into an empty base splits the vptr store into the base
// constructor and scores 4 of 10.
//
// Eight class and constructor shapes were compiled at BOTH levels before
// this and all eight put the vptr store second: a member initialiser list;
// an unused constructor parameter; a second virtual function; the zeros
// written through a named local; `enabled` as `bool` rather than `u8`; an
// empty base class carrying the virtual; `this` copied into a named local;
// and the baseline. At /O2 /Os the vtable register changes from r9 to r10
// and the transposition remains, so the flag never reached it either -- and
// with the pin in place /Os is 7 of 10, so this one wants plain /O2.
struct Listener
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ s32  unk0004;
    /* 0x08 */ s32  f08;
    /* 0x0C */ s32  f0C;
    /* 0x10 */ s32  f10;
    /* 0x14 */ s32  f14;
    /* 0x18 */ s32  f18;
    /* 0x1C */ char unk001C[0x94 - 0x1C];
    /* 0x94 */ u8   enabled;

    Listener();
    virtual void Release();
};

Listener::Listener()
{
    s32* p  = &f08;
    *p      = 0;
    f0C     = 0;
    f10     = 0;
    f14     = 0;
    enabled = 1;
    f18     = 0;
}
