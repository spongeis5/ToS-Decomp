// sub_82897128 -- release the handle at +8, if there is one. 20 B,
// 3 callers.
//
//      lwz    r3,8(r3)
//      cmplwi cr6,r3,0
//      beqlr  cr6
//      b      0x82662E08
//      (blr)                  unreachable; the recorded size may be short
//
// The callee is already matched: 82662E08 is ReleaseHandle in
// src/m_handle_release.cpp, the most-called function in the image.  So the
// field is a HANDLE and not a pointer, and this is src/null_tailcall.cpp's
// shape with the field at +8 instead of +0.
//
// The loaded value IS the argument, so the guard tests what is about to be
// passed; the tail call is the fall-through and is written first.

#include "types.h"

void ReleaseHandle(u32 h);

struct HandleAt8
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ u32  handle;
};
ASSERT_OFFSET(HandleAt8, handle, 0x08);

void ReleaseIfPresent(HandleAt8* h)
{
    u32 v = h->handle;

    if (v)
        ReleaseHandle(v);
}
