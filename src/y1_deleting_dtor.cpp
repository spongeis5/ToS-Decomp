#include "types.h"

// sub_826491E0 -- MSVC's scalar deleting destructor: reset the vtable, run
// the sub-object destructor at +12, free the storage when bit 0 of the flag
// is set, and return `this`. 96 B.
// Bridge between 826491D8 and zero64_68_0 (82649240).
//
//      mr r31,r3 ; addi r3,r3,12
//      addi r10,r11,-21480 -> 8206AC18 ; mr r30,r4
//      stw r10,0(r31) ; bl 0x8219e878        the sub-object dtor, a bare blr
//      clrlwi r9,r30,31 ; mr r3,r31
//      cmplwi cr6,r9,0 ; beq- out
//      bl 0x82603108                          the guarded free
//      mr r3,r31
//
// `clrlwi ...,31` on the second argument, a call to the image's guarded free
// under it, and `this` returned on both paths is the shape MSVC generates
// for `operator delete` inside a deleting destructor; 82603108 is
// c_release_guarded.cpp, which already null-checks and compares against two
// static objects before freeing.
//
// The two `mr r3,r31` are the return value being re-established after the
// call clobbers r3, and the one before the branch is the fall-through path's.

struct VT826491E0;
extern const VT826491E0 kVTable_8206AC18;

struct Sub826491E0
{
    /* 0x00 */ s32 unk0000;
};

void SubDestroy(Sub826491E0* s);   /* sub_8219E878 -- a bare blr */
void FreeGuarded(void* p);         /* sub_82603108 */

struct Obj826491E0
{
    /* 0x00 */ const VT826491E0* vt;
    /* 0x04 */ char              unk0004[0x08];
    /* 0x0C */ Sub826491E0       sub;
};
ASSERT_OFFSET(Obj826491E0, sub, 0x0C);

void* DeletingDestroy(Obj826491E0* o, u32 flags)
{
    o->vt = &kVTable_8206AC18;
    SubDestroy(&o->sub);

    if ((flags & 1) != 0)
        FreeGuarded(o);

    return o;
}
