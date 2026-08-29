#include "types.h"

// sub_82724A68 -- reserve space from a per-thread allocator. 44 B, 24 callers.
//
//      lwz     r11,8(r3)       count
//      li      r9,40           <- TLS slot offset, LINKER-assigned
//      lwz     r8,0(r13)       TLS base
//      li      r6,24
//      addi    r11,r11,1       count + 1
//      lwz     r4,0(r3)        owner
//      rlwinm  r10,r11,1,0,30  *2
//      add     r7,r11,r10      *3
//      lwzx    r3,r9,r8        the thread-local pointer's VALUE
//      rlwinm  r5,r7,2,0,29    *4   -> (count + 1) * 12
//      b       0x8262F658
//
// `lwz rX,0(r13)` plus a bare `li` of a small constant is `__declspec(thread)`
// on this compiler: r13 holds the thread block and the `li` carries an
// IMAGE_REL_PPC_TOCREL14 relocation for the slot offset. The 40 here is the
// LINKER's answer, not the source's -- our object emits `li r9,0` and
// build.py resolves it from the retail word.
//
// The tell that it is TLS rather than two ordinary literals is that the
// offset does not fold: the compiler will not combine a relocated immediate
// with anything, so a member offset always appears as a separate `addi`.
//
// (count + 1) * 12 is built as ((n + n*2) * 4), the same shape as the *24 in
// stride24.cpp. The multiplier is whatever the shifts and adds come to; here
// three then four.
__declspec(thread) void* t_frameHeap;

void* HeapReserve(void* heap, void* owner, int bytes, int align);

struct Reserver
{
    void* owner;
    char  unk0004[4];
    int   count;
};
ASSERT_OFFSET(Reserver, count, 8);

void* Reserve(Reserver* r)
{
    return HeapReserve(t_frameHeap, r->owner, (r->count + 1) * 12, 24);
}
