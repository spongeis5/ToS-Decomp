#include "types.h"

// sub_827841D8 -- reference-counted Release. 84 B, 16 callers.
//
//      li      r11,-1
//      addi    r7,r3,20         &this->m_ref
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
// Three things identify this as `delete this` rather than a hand-written
// destroy call:
//
//   * the null test comes AFTER the decrement, which no sane source would
//     write. MSVC emits it as part of `delete p`;
//   * the argument is a literal 1 -- the flag MSVC passes to a scalar
//     deleting destructor to say "and free the storage";
//   * the call goes through vtable SLOT 0, where MSVC puts that destructor.
//
// The interlocked shape is _InterlockedExchangeAdd, not _InterlockedDecrement:
// the constant is materialised into r11 and used with a full `add`, where the
// decrement intrinsic folds it into the reservation loop as `addi r10,r10,-1`.
// Same reasoning as c_share_static.cpp and sub_826632F8.
//
// The `-1` is applied AFTER the lwsync, which looks wrong until you notice
// the barrier orders memory and not registers, so the compiler is free to
// sink pure arithmetic past it. That is the shape of the XDK's
// InterlockedDecrementAcquire: the operation, then AcquireLockBarrier(),
// with the "return the new value" subtraction free to move.
//
// NEEDS /O2 /Os. At plain /O2 it is 18 of 21 with the right size: the
// subtraction goes to a fresh r6 and the vtable slot load to r10, where the
// target coalesces both onto r11. Same register-coalescing difference as
// m_scale_sq.cpp and c_share_static.cpp.
extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

// __lwsync is NOT a function -- ppcintrinsics.h defines it as
// `__emit(0x7C2004AC)`, the raw instruction word. Declaring it by hand as an
// extern "C" void() with `#pragma intrinsic` compiles WITHOUT A WARNING and
// emits a real `bl`, which forces a stack frame and turns the tail call into
// a `bctrl` -- 132 bytes against the target's 84. Nothing in the diagnostics
// says so; the giveaway is the `bl` in our own object.
#include <ppcintrinsics.h>

struct Releasable
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ char unk0004[16];
    /* 0x14 */ long m_ref;

    virtual ~Releasable();
    void AddRef();
    void Release();
};

// sub_827827B8 -- the twin, 40 B, 6 callers. THEY ARE 6.7 KB APART, which is
// no evidence of a shared translation unit at all; what puts them in one
// file is that both take a pointer whose reference count is at +0x14 and use
// the same reservation idiom, one adding 1 and one adding -1. That is a
// matched AddRef/Release pair, and with /Gy the two COMDATs can land
// anywhere. It is behavioural evidence for the TYPE, not for the unit -- and
// it is the type this file asserts. AddRef is flag-insensitive, so it says
// nothing about Release needing /Os. The same reservation loop with
// +1 instead of -1, the same +20 field, and the result discarded -- so no
// barrier and no test. Its constant is materialised into r11 and used with a
// full `add`, which is again _InterlockedExchangeAdd and not
// _InterlockedIncrement: the increment intrinsic would fold the 1 into the
// loop as `addi r10,r10,1` and save the register.
void Releasable::AddRef()
{
    _InterlockedExchangeAdd(&m_ref, 1);
}

void Releasable::Release()
{
    long old = _InterlockedExchangeAdd(&m_ref, -1);
    __lwsync();
    if (old - 1 == 0)
        delete this;
}
