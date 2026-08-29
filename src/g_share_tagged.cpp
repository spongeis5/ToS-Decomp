#include "types.h"

// sub_826A4528 -- take a reference on the same static shared object
// src/c_share_static.cpp uses, then store its address with bit 0 SET.
// 56 B, 14 callers.
//
//      lis     r11,-32104
//      li      r7,1
//      addi    r11,r11,2440     r11 = &g_sharedEmpty      (0x82980988)
//      addi    r6,r11,4         r6  = &...refCount        (0x8298098C)
//  L:  mfmsr   r8
//      mtmsrd  r13,1
//      lwarx   r10,0,r6
//      add     r9,r7,r10
//      stwcx.  r9,0,r6
//      mtmsrd  r8,1
//      bne+    L
//      ori     r11,r11,1        &g_sharedEmpty | 1
//      stw     r11,0(r3)
//      blr
//
// The reservation address goes into its OWN register (r6) here, where
// sub_826A3648 reuses r11 -- because r11 has to stay live past the loop to be
// OR'd. `ori` and not `addi` is a bitwise tag, so the stored word is a tagged
// pointer, not a pointer to a field one byte in.
//
// Same interlocked expansion as sub_826632F8 and sub_826A3648: the constant is
// materialised into a register and added, which is _InterlockedExchangeAdd.
//
// FLAGS: /O2 /Os. At plain /O2 this source is 12 of 14 -- identical except
// that the OR picks a fresh destination (`ori r5,r11,1` / `stw r5,0(r3)`)
// where the target updates r11 in place. Two source shapes were tried against
// that (a named `u32 tagged` mutated with `|=`, and the expression inline) and
// both compile to the same thing at either level, so the register was never a
// source property: it is the optimisation level, and this is a ninth function
// for the "flags are a property of the translation unit" list. Note that
// sub_826A3648 (src/c_share_static.cpp) touches the same global and is 3.8 KB
// away -- far enough that it says nothing about this unit's level either way,
// which is the mistake MATCHED.md records about sub_827007E8.

extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

struct SharedData
{
    u32  unk0000;
    long refCount;
};
ASSERT_OFFSET(SharedData, refCount, 0x04);

extern SharedData g_sharedEmpty;

void AttachSharedTagged(u32* slot)
{
    _InterlockedExchangeAdd(&g_sharedEmpty.refCount, 1);
    *slot = (u32)&g_sharedEmpty | 1;
}
