#include "types.h"

// sub_826A9E88 -- reference-counted Release, refcount at +0x04. 84 B, 12
// callers.
//
//      li      r11,-1
//      addi    r7,r3,4          &this->m_ref
//  L:  mfmsr   r8 ; mtmsrd r13,1
//      lwarx   r10,0,r7         r10 = the OLD value
//      add     r9,r11,r10
//      stwcx.  r9,0,r7
//      mtmsrd  r8,1
//      bne+    L
//      mr      r11,r10
//      lwsync
//      addic.  r11,r11,-1       new = old - 1, and set CR0
//      bnelr                    still referenced: return
//      cmplwi  cr6,r3,0
//      beqlr   cr6              <- a null test AFTER the decrement
//      lwz     r11,0(r3)
//      li      r4,1             <- the deleting-destructor flag
//      lwz     r11,0(r11)
//      mtctr   r11
//      bctr
//
// Instruction for instruction this is sub_827841D8 (m_release.cpp) with the
// counter at +0x04 instead of +0x14 -- so the class has nothing between the
// vptr and the refcount. The three tells for `delete this` are the same: the
// null test comes AFTER the decrement, the argument is the literal 1 that
// MSVC passes to a scalar deleting destructor, and the call goes through
// vtable slot 0.
//
// _InterlockedExchangeAdd and not _InterlockedDecrement: the constant is
// materialised into r11 and used with a full `add`, where the decrement
// intrinsic folds it into the reservation loop as `addi r10,r10,-1`.
//
// NEEDS /O2 /Os, for the same register-coalescing reason m_release.cpp does.
extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

// __lwsync is NOT a function -- declaring it by hand emits a real `bl`, a
// stack frame and a bctrl. Include the XDK header.
#include <ppcintrinsics.h>

struct RefCounted4
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ long m_ref;

    virtual ~RefCounted4();
    void Release();
};

void RefCounted4::Release()
{
    long old = _InterlockedExchangeAdd(&m_ref, -1);
    __lwsync();
    if (old - 1 == 0)
        delete this;
}
