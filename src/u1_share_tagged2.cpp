#include "types.h"

// sub_826A46C0 -- store an owner pointer, take a reference on the same static
// shared object src/g_share_tagged.cpp and src/c_share_static.cpp use, then
// store its address with bit 1 SET. 60 B, 6 callers.
//
//      lis     r11,-32104
//      stw     r4,4(r3)         s->owner = owner   -- FIRST, before the lock
//      li      r8,1
//      addi    r11,r11,2440     r11 = &g_sharedEmpty   (0x82980988)
//      addi    r6,r11,4         r6  = &...refCount     (0x8298098C)
//  L:  mfmsr   r7
//      mtmsrd  r13,1
//      lwarx   r10,0,r6
//      add     r9,r8,r10
//      stwcx.  r9,0,r6
//      mtmsrd  r7,1
//      bne+    L
//      ori     r11,r11,2        &g_sharedEmpty | 2
//      stw     r11,0(r3)
//      blr
//
// Identical to sub_826A4528 apart from the leading store and the tag bit: the
// reservation address gets its own register (r6) because r11 must stay live
// past the loop to be OR'd, and the constant 1 is materialised and ADDED,
// which is _InterlockedExchangeAdd rather than _InterlockedIncrement (see
// src/c_atomic_inc4.cpp for the distinction).
//
// `ori` and not `addi` makes the stored word a TAGGED POINTER, not a pointer
// to a field two bytes in.

extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

struct SharedData
{
    u32  unk0000;
    long refCount;
};
ASSERT_OFFSET(SharedData, refCount, 0x04);

extern SharedData g_sharedEmpty;

struct TaggedSlot
{
    /* 0x00 */ u32   tagged;
    /* 0x04 */ void* owner;
};
ASSERT_OFFSET(TaggedSlot, owner, 0x04);

void AttachSharedTagged2(TaggedSlot* s, void* owner)
{
    s->owner = owner;
    _InterlockedExchangeAdd(&g_sharedEmpty.refCount, 1);
    s->tagged = (u32)&g_sharedEmpty | 2;
}
