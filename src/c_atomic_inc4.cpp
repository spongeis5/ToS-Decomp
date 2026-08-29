// sub_826632F8 -- interlocked add of 1 to the word at +4. 40 B, 31 callers.
//
//   li     r11,1
//   addi   r7,r3,4
// L:mfmsr  r8
//   mtmsrd r13,1
//   lwarx  r10,0,r7
//   add    r9,r11,r10
//   stwcx. r9,0,r7
//   mtmsrd r8,1
//   bne+   L
//   blr
//
// The mfmsr / mtmsrd r13,1 ... mtmsrd r8,1 bracket around the lwarx/stwcx.
// pair is the XDK compiler's own expansion of the interlocked intrinsics on
// Xenon; the source does not spell it out.
//
// It is _InterlockedExchangeAdd, NOT _InterlockedIncrement. The increment
// intrinsic folds the constant into the reservation loop as `addi r10,r10,1`
// and needs no separate register; this target materialises 1 into r11 and
// uses a full `add`, which is the generic add-a-value form, and it costs the
// extra instruction that makes the body 40 bytes rather than 36.
//
// r3 is never written, so the previous value is discarded.

#include "types.h"

extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

struct RefCounted
{
    u32  unk0000;
    long refCount;
};
ASSERT_OFFSET(RefCounted, refCount, 0x04);

void AddRef(RefCounted* p)
{
    _InterlockedExchangeAdd(&p->refCount, 1);
}
