#include "types.h"

// sub_82663320 -- interlocked decrement of the refcount at +4, then
// `delete this` at zero. 80 B.
// Bridge between AddRef (826632F8) and ReleaseNode (82663370), and it is
// c_atomic_inc4.cpp's object with the other sign.
//
//      li     r11,-1 ; addi r7,r3,4
//   L: mfmsr r8 ; mtmsrd r13,1 ; lwarx r10,0,r7
//      add    r9,r11,r10 ; stwcx. r9,0,r7 ; mtmsrd r8,1 ; bne+ L
//      mr     r11,r10 ; addic. r11,r11,-1 ; bnelr
//      cmplwi cr6,r3,0 ; beqlr cr6
//      lwz    r11,0(r3) ; li r4,1 ; lwz r11,0(r11) ; mtctr ; bctr
//
// The mfmsr / mtmsrd bracket is the XDK compiler's own expansion of the
// interlocked intrinsics; the source does not spell it out. It is
// _InterlockedExchangeAdd and not _InterlockedDecrement for the reason
// c_atomic_inc4.cpp records: the decrement intrinsic folds the constant into
// the reservation loop, while this one materialises -1 into r11 and uses a
// full `add`.
//
// THE TAIL IS `delete p`, NOT A HAND-WRITTEN CALL. A null test followed by
// vtable slot 0 called with a second argument of 1 is MSVC's expansion of
// `delete` on a class with a virtual destructor -- slot 0 is the vector
// deleting destructor and the 1 is its "also free the storage" flag. That
// also fixes the layout: the vptr is at +0 and the refcount at +4.

extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

struct RefCounted
{
    /* 0x00 */ virtual ~RefCounted();
    /* 0x04 */ long refCount;
};

void Release(RefCounted* p)
{
    if (_InterlockedExchangeAdd(&p->refCount, -1) - 1 == 0)
        delete p;
}
