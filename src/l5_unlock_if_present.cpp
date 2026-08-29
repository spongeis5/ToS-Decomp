// sub_826770F8 -- release the object's lock, if it has one. 20 B, 4 callers.
//
//   lwz    r3,180(r3)
//   cmplwi cr6,r3,0
//   beqlr  cr6
//   b      0x8291285C
//   (blr)                     unreachable; the recorded size may be short
//
// 8291285C is an XEX import thunk, and build/imports.txt names it:
// xboxkrnl.exe ordinal 304, RtlLeaveCriticalSection.  So this is the release
// half of the pair whose acquire half is sub_826770C8 at the same field
// offset 0xB4, 0x30 bytes earlier.
//
// Same shape as src/null_tailcall.cpp: the loaded pointer is the argument, so
// the guard tests the value about to be passed and the tail call is the
// fall-through.

#include "types.h"

struct CriticalSection;

extern "C" void RtlLeaveCriticalSection(CriticalSection* cs);

struct LockedObject
{
    /* 0x00 */ char             unk0000[0xB4];
    /* 0xB4 */ CriticalSection* lock;
};
ASSERT_OFFSET(LockedObject, lock, 0xB4);

void UnlockIfPresent(LockedObject* o)
{
    CriticalSection* cs = o->lock;
    if (cs)
        RtlLeaveCriticalSection(cs);
}
