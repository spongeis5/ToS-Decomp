#include "types.h"

// sub_82662EA0 -- 124 B, 161 callers. A BRIDGE between sub_82662E08
// (src/m_handle_release.cpp, ReleaseHandle) and sub_82662F20
// (src/c_handle_lookup.cpp), and it is literally the two of them fused:
// the handle decode of the second feeding the lock/create/unlock of
// AcquireEntry (sub_82662938, in m_handle_release.cpp).
//
//      mflr    r12
//      bl      0x828A75CC          __savegprlr_29 -- r29..r31 and LR
//      stwu    r1,-112(r1)
//      lis     r11,-32091
//      rlwinm  r10,r3,15,17,28     (h >> 20) * 8
//      rlwinm  r9,r3,22,22,29      ((h >> 12) & 0xFF) * 4
//      mr      r29,r4              spec, live across both calls
//      lwz     r11,16748(r11)      g_handlePages        (0x82A5416C)
//      add     r11,r10,r11
//      lwz     r11,4(r11)          page->slots
//      lwzx    r11,r11,r9          slots[(h >> 12) & 0xFF]
//      lwz     r31,20(r11)         entry->owner
//      lbz     r11,100(r31)        owner->threadSafe
//      cmplwi  r11,0
//      beq-    plain
//      addi    r30,r31,72          &owner->lock
//      mr      r3,r30  ; bl 0x8291284C          enter
//      mr      r4,r29 ; lwz r3,104(r31) ; bl 0x826D9838
//      mr      r31,r3              the result survives the unlock
//      mr      r3,r30  ; bl 0x8291285C          leave
//      mr      r3,r31 ; b out
// plain:mr     r4,r29 ; lwz r3,104(r31) ; bl 0x826D9838
// out: addi    r1,r1,112
//      b       0x828A761C          __restgprlr_29
//
// Register discipline, and it is the same as AcquireEntry's word for word:
// three callee-saved registers (hence __savegprlr_29 rather than _28), r31
// holding the owner and then reused for the result, r30 the lock address,
// r29 the spec. The one difference from AcquireEntry is that the owner
// arrives through the two-level page table instead of in r3, and the whole
// decode chain COALESCES onto r11 -- page base, page, slots, entry -- which
// is the /Os signature that m_handle_release.cpp already records for the
// same chain in ReleaseHandle.
//
// The strides are read off the two rlwinms exactly as c_handle_lookup.cpp
// reads them: MASK(17,28) with ROTL 15 scales by 8, MASK(22,29) with ROTL
// 22 scales by 4.
//
// The types are repeated from src/m_handle_release.cpp field for field and
// under the SAME NAMES, deliberately: 0x8291284C, 0x8291285C and 0x826D9838
// are the same three retail functions both files call, so a second spelling
// of `CritSec` or `HandlePageR` would make build.py's NAME DRIFT report say
// one function is two. `activeHandle` at 0x14 is not read here -- it is
// carried across from ReleaseHandle's measurement, not re-asserted from
// these bytes. What is NOT merged is c_handle_lookup.cpp's `HandleObject`,
// which models the +0x14 field as a `u32 value` where this needs a pointer;
// that file says one of the two is wrong and nothing yet says which.

struct HandleOwner;

struct HandleEntry
{
    char          unk0000[0x14];
    HandleOwner*  owner;
};
ASSERT_OFFSET(HandleEntry, owner, 0x14);

struct HandlePageR
{
    u32           unk0000;
    HandleEntry** slots;
};
ASSERT_OFFSET(HandlePageR, slots, 0x04);
ASSERT_SIZE(HandlePageR, 8);

struct CritSec
{
    char unk0000[4];
};

struct HandleOwner
{
    /* 0x00 */ char     unk0000[0x14];
    /* 0x14 */ u32      activeHandle;
    /* 0x18 */ char     unk0018[0x48 - 0x18];
    /* 0x48 */ CritSec  lock;
    /* 0x4C */ char     unk004C[0x64 - 0x4C];
    /* 0x64 */ u8       threadSafe;
    /* 0x65 */ char     unk0065[3];
    /* 0x68 */ void*    context;
};
ASSERT_OFFSET(HandleOwner, activeHandle, 0x14);
ASSERT_OFFSET(HandleOwner, lock, 0x48);
ASSERT_OFFSET(HandleOwner, threadSafe, 0x64);
ASSERT_OFFSET(HandleOwner, context, 0x68);

extern HandlePageR* g_handlePages;

void LockEnter(CritSec* cs);
void LockLeave(CritSec* cs);
u32  CreateEntry(void* context, void* spec);

u32 AcquireByHandle(u32 h, void* spec)
{
    HandleOwner* o = g_handlePages[h >> 20].slots[(h >> 12) & 0xFF]->owner;

    if (o->threadSafe)
    {
        LockEnter(&o->lock);
        u32 r = CreateEntry(o->context, spec);
        LockLeave(&o->lock);
        return r;
    }
    return CreateEntry(o->context, spec);
}
