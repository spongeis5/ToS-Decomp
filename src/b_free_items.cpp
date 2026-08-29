// sub_82637590 -- release an object's element array back to the thread's
// allocator, unless a top-bit flag says it does not own it.
// 56 bytes, 37 callers.
//
//      lwz     r11,4(r3)           this->flags
//      rlwinm  r10,r11,0,0,0       & 0x80000000
//      cmpwi   cr6,r10,0
//      bnelr   cr6                 flag set: not ours to free
//      lwz     r11,8(r3)           this->count
//      li      r10,40              TLS slot offset (link-time value)
//      lwz     r9,0(r13)           thread block
//      li      r6,24               tag
//      addi    r8,r11,1
//      lwz     r4,0(r3)            this->items
//      rlwinm  r5,r8,3,0,28        (count + 1) * 8
//      lwzx    r3,r10,r9           g_allocator
//      b       0x8262F658          tail call
//
// `rlwinm rX,rY,0,0,0` keeps ONLY bit 0, which on this big-endian target is
// the 0x80000000 mask -- a flag test, not a sign test (a sign test would be
// `cmpwi`/`bltlr` with no mask at all).
//
// The tail call is the same routine sub_8262FB50 reaches, with the same
// four-argument shape (allocator, block, byte count, tag); this caller knows
// the size itself instead of reading it out of a block header, so the two
// are different entry paths into one release routine.
//
// `li 40` + `lwz 0(r13)` + `lwzx` is the Xbox 360 __declspec(thread) read:
// r13 holds the thread block pointer and the un-folded `li` carries the
// linker-assigned slot offset. Compare src/a_tls_field.cpp, which takes the
// ADDRESS of a field of a thread-local and so keeps a second addi; this one
// loads the variable's value, so the offset goes straight into the lwzx.

#include "types.h"

struct Allocator;

struct Object
{
    /* 0x00 */ void* items;
    /* 0x04 */ s32   flags;
    /* 0x08 */ u32   count;
};

ASSERT_OFFSET(Object, items, 0x00);
ASSERT_OFFSET(Object, flags, 0x04);
ASSERT_OFFSET(Object, count, 0x08);

__declspec(thread) extern Allocator* g_allocator;

void ReleaseBlock(Allocator* a, void* block, u32 bytes, u32 tag);

void FreeItems(Object* o)
{
    if ((o->flags & (s32)0x80000000) != 0)
        return;
    ReleaseBlock(g_allocator, o->items, (o->count + 1) * 8, 24);
}
