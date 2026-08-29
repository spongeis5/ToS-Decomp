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
