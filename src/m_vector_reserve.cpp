#include "types.h"

// sub_82667E58 -- reserve an explicit capacity. 136 B, 132 callers.
//
// The sibling of sub_82667EE0 (src/m_vector_grow.cpp): same struct, same two
// allocator calls, same memcpy, same packed word at +8 with the same three
// masks. The only difference is that the new capacity ARRIVES as a parameter
// instead of being computed as `count ? count * 2 : 1`, so this is Reserve
// and that one is Grow.
//
//      mflr r12 ; bl 0x828A75C0 ; stwu r1,-144(r1)
//      lwz     r28,0(r13) ; li r29,40      the allocator, TLS slot 40
//      mr      r30,r5      elemSize
//      mr      r27,r4      newCap
//      li      r5,24
//      mullw   r4,r4,r30
//      mr      r31,r3
//      lwzx    r3,r29,r28
//      bl      0x8262F5D0   BinAlloc(a, newCap * elemSize, 24)
//      lwz     r11,4(r31)   count
//      lwz     r4,0(r31)
//      mr      r26,r3
//      mullw   r5,r11,r30
//      bl      0x828A8CF0   memcpy(new, old, count * elemSize)
//      lwz     r11,8(r31)
//      rlwinm  r10,r11,0,0,0 ; cmpwi cr6,r10,0 ; bne- store
//      clrlwi  r11,r11,2
//      lwz     r4,0(r31) ; li r6,24 ; lwzx r3,r29,r28
//      mullw   r5,r11,r30
//      bl      0x8262F658   BinFree(a, old, oldCap * elemSize, 24)
// store:lwz    r11,8(r31)
//      stw     r26,0(r31)
//      rlwinm  r10,r11,0,1,1
//      or      r9,r10,r27
//      stw     r9,8(r31)
//
// WRITTEN TO SETTLE A QUESTION as much as to match, AND IT SETTLED IT.
// VectorGrow below is one word short on the operand order of
// `mullw r5,<count>,<elemSize>`, and nine source shapes could not move it.
// Here the identical multiply, with NO earlier read of `count` anywhere in
// the function, comes out right first time.
//
// So the displacement is caused by the earlier read: in Grow, `v->count` is
// read once for the doubling and once for the memcpy, MSVC makes the first
// the CSE representative, and the operand whose "source read comes later"
// then looks like elemSize rather than count. Reserve has one read and gets
// it right. That is the mechanism behind the `add`-operand-order lever, seen
// from the other side -- and it is not something the source can undo, which
// is why Grow stays at 31 of 38.
//
// Both are written against ONE shared __forceinline implementation, which is
// how the original almost certainly reads: they are adjacent in the image
// (82667E58 + 136 = 82667EE0), they differ only in where the new capacity
// comes from, and the shared form compiles to exactly the same bytes as
// writing the body out twice.
struct BinnedAllocator;
struct FreeBlock;

FreeBlock* BinAlloc(BinnedAllocator* a, int size, int tag);
void       BinFree(BinnedAllocator* a, FreeBlock* p, int size, int tag);

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

__declspec(thread) BinnedAllocator* t_binAllocator;

struct ReserveVector
{
    /* 0x00 */ void* data;
    /* 0x04 */ s32   count;
    /* 0x08 */ s32   capacityAndFlags;
};
ASSERT_OFFSET(ReserveVector, count, 0x04);
ASSERT_OFFSET(ReserveVector, capacityAndFlags, 0x08);

static __forceinline void ReserveImpl(ReserveVector* v, s32 newCap,
                                      int elemSize)
{
    void* buf = BinAlloc(t_binAllocator, newCap * elemSize, 24);
    memcpy(buf, v->data, v->count * elemSize);

    if ((v->capacityAndFlags & (s32)0x80000000) == 0)
        BinFree(t_binAllocator, (FreeBlock*)v->data,
                (v->capacityAndFlags & 0x3FFFFFFF) * elemSize, 24);

    v->data = buf;
    v->capacityAndFlags = (v->capacityAndFlags & 0x40000000) | newCap;
}

void VectorReserve(ReserveVector* v, s32 newCap, int elemSize)
{
    ReserveImpl(v, newCap, elemSize);
}

// sub_82667EE0 -- grow. 152 B, 180 callers, and it sits IMMEDIATELY AFTER
// Reserve in the image: 82667E58 + 136 = 82667EE0. Written as a call to
// Reserve, which MSVC inlines -- and the definition has to come FIRST for
// that to happen: declared-but-not-yet-defined it emits a real `bl` and an
// eight-word body.
void VectorGrow(ReserveVector* v, int elemSize)
{
    ReserveImpl(v, v->count ? v->count * 2 : 1, elemSize);
}
