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
// NOT MATCHED: 8 of 10 words. The two that differ are the FIRST TWO STORES,
// transposed and nothing else -- we emit `stw r11,8(r3)` where the target
// emits `stw r9,0(r3)`, and vice versa. The four setup instructions ahead of
// them (lis, li r11,0, addi r9, li r8,1) are identical in both, so the
// vtable address is ready at exactly the same cycle in both and this is not
// the gap-filling exception above: it is a pure TIE-BREAK between two ready
// stores. The target issues the one whose value was defined LATER (r9, from
// the addi at slot 3); we issue the one defined EARLIER (r11, from the li at
// slot 2).
//
// Eight class and constructor shapes were compiled at BOTH levels and all
// eight put the vptr store second: a member initialiser list; an unused
// constructor parameter; a second virtual function; the zeros written
// through a named local; `enabled` as `bool` rather than `u8`; an empty base
// class carrying the virtual (which splits the vptr store into the base
// constructor and is not this function); `this` copied into a named local;
// and the baseline. At /O2 /Os the vtable register changes from r9 to r10
// and the transposition remains, so the flag does not reach it either.
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
    f08     = 0;
    f0C     = 0;
    f10     = 0;
    f14     = 0;
    enabled = 1;
    f18     = 0;
}
