#include "types.h"

// sub_82662E08 -- THE MOST-CALLED FUNCTION IN THE IMAGE: 730 callers, 152 B.
//
// It was not in the candidate list because `tools/candidates.py` only offers
// LEAF functions, and this one makes four calls. Ranking the whole inventory
// by caller count instead is what surfaced it.
//
//      mflr    r12
//      bl      0x828A75C8          __savegprlr_28 -- saves r28..r31 and LR
//      stwu    r1,-128(r1)
//      mr      r30,r3
//      cmplwi  cr6,r3,0
//      beq-    cr6,end
//      lis     r11,-32091
//      rlwinm  r10,r3,15,17,28     (h >> 20) * 8
//      rlwinm  r9,r3,22,22,29      ((h >> 12) & 0xFF) * 4
//      lwz     r11,16748(r11)      g_handlePages          (0x82A5416C)
//      add     r11,r10,r11
//      lwz     r11,4(r11)          page->slots
//      lwzx    r29,r11,r9          slots[(h >> 12) & 0xFF]
//      lwz     r31,20(r29)         obj->owner
//      lbz     r11,100(r31)        owner->threadSafe
//      cmplwi  r11,0
//      beq-    plain
//      addi    r28,r31,72          &owner->lock
//      mr      r3,r28  ; bl 0x8291284C          enter
//      mr      r5,r30 ; mr r4,r29 ; lwz r3,104(r31) ; bl 0x826D9B70
//      mr      r3,r28  ; bl 0x8291285C          leave
//      b       after
// plain:mr     r5,r30 ; lwz r3,104(r31) ; mr r4,r29 ; bl 0x826D9B70
// after:lwz    r11,20(r31)
//      cmplw   cr6,r30,r11
//      bne-    cr6,end
//      mr      r3,r31
//      bl      0x82662CB8
// end: addi    r1,r1,128
//      b       0x828A7618          __restgprlr_28
//
// The handle decode is the same two-level table src/c_handle_lookup.cpp
// models, off the same global at 0x82A5416C, with the strides measured out
// of the shift-and-mask rather than guessed: MASK(17,28) with ROTL 15 scales
// by 8, MASK(22,29) with ROTL 22 scales by 4. The types are declared again
// here rather than shared, because that file models the +0x14 field as a
// `u32 value` and this one needs it as a pointer -- one of them is wrong
// about the type and nothing yet says which, so merging would invent an
// identity instead of recording one.
//
// The notify call is emitted TWICE, once in each arm, which is the source
// having written it twice rather than the compiler duplicating a tail. The
// two copies schedule their three argument moves differently, which is the
// scheduler and not the source.
//
// NEEDS /O2 /Os, by the signature that keeps recurring: at plain /O2 the
// three chained loads each take a FRESH destination (r8, r7, r6) where the
// target coalesces all of them onto r11, and the byte test lands in cr6
// instead of cr0. 23 of 38 there, 29 of 29 non-relocated words here.
//
// It is also another adjacency data point: this is 0x118 before
// sub_82662F20 (src/c_handle_lookup.cpp), which is also /Os only, and the
// run continues through 82663260 and 82663370.
//
// `bl __savegprlr_28` / `b __restgprlr_28` are the XDK's register save and
// restore helpers; the compiler emits them for any function that keeps four
// values live across a call. Nothing in the source asks for them -- keeping
// the handle, the object, the owner and the lock address live is what does.

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
void NotifyReleased(void* context, HandleEntry* e, u32 h);
void OwnerFinished(HandleOwner* o);
u32  CreateEntry(void* context, void* spec);

void ReleaseHandle(u32 h)
{
    if (h == 0)
        return;

    HandleEntry* e = g_handlePages[h >> 20].slots[(h >> 12) & 0xFF];
    HandleOwner* o = e->owner;

    if (o->threadSafe)
    {
        LockEnter(&o->lock);
        NotifyReleased(o->context, e, h);
        LockLeave(&o->lock);
    }
    else
    {
        NotifyReleased(o->context, e, h);
    }

    if (h == o->activeHandle)
        OwnerFinished(o);
}

// sub_82662938 -- the acquire counterpart. 96 B, 367 callers.
//
//      mflr    r12 ; bl 0x828A75CC (__savegprlr_29) ; stwu r1,-112(r1)
//      lbz     r11,100(r3)     owner->threadSafe
//      mr      r31,r3 ; mr r29,r4
//      cmplwi  r11,0 ; beq- plain
//      addi    r30,r3,72       &owner->lock
//      mr      r3,r30 ; bl enter
//      mr      r4,r29 ; lwz r3,104(r31) ; bl 0x826D9838
//      mr      r31,r3          keep the result across the unlock
//      mr      r3,r30 ; bl leave
//      mr      r3,r31 ; b out
// plain:mr     r4,r29 ; lwz r3,104(r31) ; bl 0x826D9838
// out: addi    r1,r1,112 ; b 0x828A761C (__restgprlr_29)
//
// SHARES THE HandleOwner TYPE ABOVE, and that is a merge on evidence rather
// than on proximity: it reads the same +0x64 flag, brackets with the same
// two lock routines around the same +0x48 field, and calls through the same
// +0x68 context. Three offsets and two callees agreeing is the strongest
// same-type signal available here. They are 0x4D0 apart, which by itself
// would say nothing.
//
// Three callee-saved registers rather than four, so __savegprlr_29 instead
// of _28 -- the count is chosen by the compiler from what stays live, and
// here the result has to survive the unlock while the lock address and the
// owner do too.
u32 AcquireEntry(HandleOwner* o, void* spec)
{
    if (o->threadSafe)
    {
        LockEnter(&o->lock);
        u32 h = CreateEntry(o->context, spec);
        LockLeave(&o->lock);
        return h;
    }
    return CreateEntry(o->context, spec);
}
