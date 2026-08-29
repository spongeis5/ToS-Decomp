// sub_82638B48 -- hkpPhysicsSystem::hkpPhysicsSystem(). 92 B, 4 callers.
//
// THIS ONE IS NAMED, NOT DESCRIBED.  The word stored at +0 is 82068E0C, and
// build/vtables.txt has that address as the vtable of `.?AVhkpPhysicsSystem@@`
// recovered from MSVC RTTI.
//
//      lis  r11,-32249 ; li r9,1 ; addi r8,r11,-29172   = 82068E0C
//      li   r11,0
//      sth  r9,6(r3)                 m_referenceCount = 1
//      lis  r10,-32768               0x80000000
//      stw  r8,0(r3)                 the vtable
//      stw  r11,8(r3)  ; stw r11,12(r3) ; stw r10,16(r3)
//      stw  r11,20(r3) ; stw r11,24(r3) ; stw r10,28(r3)
//      stw  r11,32(r3) ; stw r11,36(r3) ; stw r10,40(r3)
//      stw  r11,44(r3) ; stw r11,48(r3) ; stw r10,52(r3)
//      stw  r11,56(r3) ; stw r11,60(r3)
//      stb  r9,64(r3)
//
// FOUR TRIPLES OF {0, 0, 0x80000000} ARE FOUR EMPTY hkArrays.  hkArray is
// {T* m_data; int m_size; int m_capacityAndFlags;} and an array that owns no
// storage carries DONT_DEALLOCATE_FLAG, the top bit, in the third word.  That
// is what identifies the layout: 0x80000000 is a flag word, not a float and
// not a sentinel, and the stride between the triples is 12.
//
// hkpPhysicsSystem in Havok 6.5 holds exactly four such arrays -- rigid
// bodies, constraints, actions, phantoms -- followed by a name, a user data
// word and an active flag, which is what +56, +60 and +64 are.
//
// IT HAS TO BE A REAL CONSTRUCTOR, and the store order is the evidence.
// Written flat -- reference count, vtable, then the fifteen member words in
// address order -- MSVC hoists the two zero stores at +8 and +12 ahead of
// both the `sth` and the vtable store, and materialises the vtable address
// through two registers instead of reusing the `lis` result: 14 of 23.
// As a constructor the order is fixed by the language: the hkReferencedObject
// base runs first (its `sth` at +6, and its own vptr store, which MSVC then
// deletes as dead), then this class's vptr, then the members in DECLARATION
// order.  +4 is the base's m_memSizeAndFlags and is deliberately never
// written -- the allocator owns it.
//
// The four arrays are default-constructed and the last three fields are
// member initialisers, not body assignments: a body assignment would run
// after every array and put +56/+60/+64 in the wrong place.

#include "types.h"

struct hkArrayAny
{
    /* 0x00 */ void* data;
    /* 0x04 */ s32   size;
    /* 0x08 */ s32   capacityAndFlags;

    hkArrayAny() : data(0), size(0), capacityAndFlags((s32)0x80000000) {}
};
ASSERT_SIZE(hkArrayAny, 12);

struct hkReferencedObject
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ u16 memSizeAndFlags;
    /* 0x06 */ u16 referenceCount;

    hkReferencedObject() : referenceCount(1) {}
    virtual ~hkReferencedObject();
};

struct hkpPhysicsSystem : public hkReferencedObject
{
    /* 0x08 */ hkArrayAny rigidBodies;
    /* 0x14 */ hkArrayAny constraints;
    /* 0x20 */ hkArrayAny actions;
    /* 0x2C */ hkArrayAny phantoms;
    /* 0x38 */ char*      name;
    /* 0x3C */ u32        userData;
    /* 0x40 */ u8         active;

    hkpPhysicsSystem();
};
ASSERT_OFFSET(hkpPhysicsSystem, rigidBodies, 0x08);
ASSERT_OFFSET(hkpPhysicsSystem, constraints, 0x14);
ASSERT_OFFSET(hkpPhysicsSystem, actions,     0x20);
ASSERT_OFFSET(hkpPhysicsSystem, phantoms,    0x2C);
ASSERT_OFFSET(hkpPhysicsSystem, name,        0x38);
ASSERT_OFFSET(hkpPhysicsSystem, userData,    0x3C);
ASSERT_OFFSET(hkpPhysicsSystem, active,      0x40);

hkpPhysicsSystem::hkpPhysicsSystem()
  : name(0),
    userData(0),
    active(1)
{
}
