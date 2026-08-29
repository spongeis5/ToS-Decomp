// sub_826770C8 -- take the object's lock, if it has one. 20 B, 4 callers.
//
//   lwz    r3,180(r3)
//   cmplwi cr6,r3,0
//   beqlr  cr6
//   b      0x826C2828
//   (blr)                     unreachable; the recorded size may be short
//
// The loaded pointer IS the argument, so the guard tests the value about to
// be passed -- the same shape as src/null_tailcall.cpp, with the field at
// 0xB4 instead of 0.
//
// The callee is not invented: sub_826C2828 pushes the profiler scope name
// "TtCriticalLock" (string at 82079400) and calls RtlTryEnterCriticalSection
// (8291319C) and RtlEnterCriticalSection (8291284C), so it takes a critical
// section by pointer.  Its sibling sub_826770F8 (src/l5_unlock_if_present.cpp)
// tail-calls RtlLeaveCriticalSection on the SAME field, which is what makes
// this field a lock rather than any pointer.
//
// beqlr returns with r3 already zero, so nothing distinguishes a void return
// from an int one here; void is the weaker claim.

#include "types.h"

struct CriticalSection;

void TtCriticalLock(CriticalSection* cs);

struct LockedObject
{
    /* 0x00 */ char             unk0000[0xB4];
    /* 0xB4 */ CriticalSection* lock;
};
ASSERT_OFFSET(LockedObject, lock, 0xB4);

void LockIfPresent(LockedObject* o)
{
    CriticalSection* cs = o->lock;
    if (cs)
        TtCriticalLock(cs);
}
