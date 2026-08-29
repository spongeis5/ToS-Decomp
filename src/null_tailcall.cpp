// sub_826A3350 -- load, null-check, tail call. 16 bytes recorded, 19 callers.
//
//      lwz     r3,0(r3)
//      cmplwi  cr6,r3,0
//      beqlr   cr6
//      b       0x82662E08
//
// The loaded pointer becomes the argument, so the guard tests the value that
// is about to be passed. Ends in an unconditional branch, so the recorded
// size may be short by the unreachable trailing blr -- match.py reconciles.

#include "types.h"

// The callee resolves to 82662E08, which src/m_handle_release.cpp matches as
// `u32 ReleaseHandle(u32)`. So the field is a HANDLE, not a pointer, and the
// name here was invented for a function whose real one is now known.
// build.py's name-drift check is what surfaced that -- until the callee was
// matched too, nothing could have.
struct Holder { /* 0x00 */ u32 handle; };

ASSERT_OFFSET(Holder, handle, 0x00);

void ReleaseHandle(u32 h);

void ReleaseIfPresent(Holder* h)
{
    u32 v = h->handle;
    if (v)
        ReleaseHandle(v);
}
