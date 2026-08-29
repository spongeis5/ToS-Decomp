#include "types.h"

// sub_8262F658 -- return a block to a size-binned free list. 164 B, 420
// callers, the second-most-called function in the image.
//
// ITS RECORDED SIZE IS 68. The inventory lists 8262F69C as a function start,
// which truncates this row at the `b 0x8262F69C` in the middle of it. It is
// not a start: control falls into it from the `lwzx` before, so it is a
// label where the two index computations join. See FINDINGS 7u -- 14.1% of
// discovery's starts are like that, against 0.4% for .pdata.
//
//      cmplwi  cr6,r4,0 ; beqlr cr6
//      cmpwi   cr6,r5,8192 ; bgt- cr6,big
//      cmpwi   cr6,r5,544  ; bgt- cr6,large
//      addi    r11,r5,15
//      srawi   r11,r11,4          (size + 15) >> 4
//      add     r10,r11,r3
//      lbz     r9,260(r10)        smallClass[...]
//      extsb   r11,r9
//      b       join
// large:addi   r11,r5,-1
//      srawi   r11,r11,10         (size - 1) >> 10
//      addi    r10,r11,74
//      rlwinm  r9,r10,2,0,29      (... + 74) * 4
//      lwzx    r11,r9,r3          largeClass[...]
// join:rlwinm  r10,r11,3,0,28     idx * 8
//      lwz     r9,52(r3)
//      add     r8,r10,r3
//      lwz     r7,60(r8)          bins[idx].count
//      cmpw    cr6,r7,r9 ; blt- cr6,push
//      mr      r5,r4 ; mr r4,r11
//      b       0x8262F3C0         SpillBin(a, idx, p)
// push:addi    r11,r11,7
//      rlwinm  r11,r11,3,0,28     (idx + 7) * 8   == idx * 8 + 56
//      add     r11,r11,r3
//      lwz     r10,4(r11) ; lwz r9,0(r11)
//      addi    r8,r10,1
//      stw     r8,4(r11) ; stw r9,0(r4) ; stw r4,0(r11)
//      blr
// big: lwz     r3,16(r3)
//      lwz     r11,0(r3) ; lwz r10,16(r11) ; mtctr r10 ; bctr
//
// THE LAYOUT IS MEASURED OUT OF THE INDEX ARITHMETIC, not guessed:
//
//   `(idx + 7) * 8` and `idx * 8 + 60` are the same base, so the bins are an
//   array of 8-byte pairs at 7 * 8 = 56, with head at +0 and count at +4.
//   The two tables follow at 260 and at (74 - 56/8) * 4 = 296, and 296 - 260
//   is 36 -- exactly the number of byte entries `(544 + 15) >> 4` can reach.
//   Eight word entries follow, which is what `(8192 - 1) >> 10` can reach.
//   Every bound in the struct comes from a constant in the code.
//
// The shifts are `srawi`, arithmetic, so the size is a signed int and the
// small table holds signed chars -- `extsb` after every `lbz` says so.
struct FreeBlock
{
    FreeBlock* next;
};

struct Bin
{
    FreeBlock* head;
    s32        count;
};
ASSERT_SIZE(Bin, 8);

struct BigVT;
struct BigHeap
{
    const BigVT* vt;
};
struct BigVT
{
    void*       slot0;
    void*       slot1;
    void*       slot2;
    FreeBlock* (*Alloc)(BigHeap* h, int size);
    void       (*Free)(BigHeap* h, FreeBlock* p, int size);
};
ASSERT_OFFSET(BigVT, Alloc, 12);
ASSERT_OFFSET(BigVT, Free, 16);

struct BinnedAllocator
{
    /* 0x000 */ char     unk0000[0x10];
    /* 0x010 */ BigHeap* big;
    /* 0x014 */ char     unk0014[0x34 - 0x14];
    /* 0x034 */ s32      keepLimit;
    /* 0x038 */ Bin      bins[25];
    /* 0x100 */ char     unk0100[4];
    /* 0x104 */ s8       smallClass[36];
    /* 0x128 */ s32      largeClass[8];
};
ASSERT_OFFSET(BinnedAllocator, big, 0x10);
ASSERT_OFFSET(BinnedAllocator, keepLimit, 0x34);
ASSERT_OFFSET(BinnedAllocator, bins, 0x38);
ASSERT_OFFSET(BinnedAllocator, smallClass, 0x104);
ASSERT_OFFSET(BinnedAllocator, largeClass, 0x128);

void SpillBin(BinnedAllocator* a, int idx, FreeBlock* p);
FreeBlock* RefillBin(BinnedAllocator* a, int idx);

void BinFree(BinnedAllocator* a, FreeBlock* p, int size, int tag)
{
    if (p == 0)
        return;

    if (size <= 8192)
    {
        int idx;
        if (size <= 544)
            idx = a->smallClass[(size + 15) >> 4];
        else
            idx = a->largeClass[(size - 1) >> 10];

        if (a->bins[idx].count >= a->keepLimit)
        {
            SpillBin(a, idx, p);
            return;
        }

        Bin* b = &a->bins[idx];
        b->count = b->count + 1;
        p->next = b->head;
        b->head = p;
    }
    else
    {
        BigHeap* h = a->big;
        h->vt->Free(h, p, size);
    }
}

// sub_8262F5D0 -- the allocate counterpart. 132 B, 206 callers.
//
// Same two size thresholds, same two class tables at the same two offsets,
// same 8-byte bins at +0x38, and the same big-heap pointer at +0x10 reached
// through vtable slot 3 where the free path uses slot 4. That is six shared
// facts about one layout, which is why it belongs in this file rather than
// declaring the struct a second time.
//
// The recorded size is 60, truncated the same way BinFree's was: 8262F60C is
// listed as a function start and control falls into it from the `extsb`
// above. Both halves of one allocator, both cut at the join of their two
// index computations.
//
//      cmpwi   cr6,r4,8192 ; bgt- big
//      cmpwi   cr6,r4,544  ; bgt- large
//      ... smallClass ...  ; b join
// large:... largeClass ...
// join:addi    r11,r4,7
//      rlwinm  r11,r11,3,0,28
//      add     r11,r11,r3
//      lwz     r10,0(r11)      bins[idx].head
//      cmplwi  cr6,r10,0
//      beq-    cr6,refill
//      lwz     r9,4(r11) ; mr r3,r10 ; addi r9,r9,-1
//      stw     r9,4(r11) ; lwz r8,0(r10) ; stw r8,0(r11)
//      blr
// refill:b     0x8262F2F0
// big: lwz     r3,16(r3) ; lwz r11,0(r3) ; lwz r10,12(r11) ; mtctr ; bctr
//
// The `beq-` jumps AWAY to the refill, so the POP is the fall-through and
// has to be written first -- the guard is `if (head != 0)`, not
// `if (head == 0) return Refill(...)`.
FreeBlock* BinAlloc(BinnedAllocator* a, int size, int tag)
{
    if (size <= 8192)
    {
        int idx;
        if (size <= 544)
            idx = a->smallClass[(size + 15) >> 4];
        else
            idx = a->largeClass[(size - 1) >> 10];

        Bin* b = &a->bins[idx];
        FreeBlock* head = b->head;
        if (head != 0)
        {
            b->count = b->count - 1;
            b->head = head->next;
            return head;
        }
        return RefillBin(a, idx);
    }

    BigHeap* h = a->big;
    return h->vt->Alloc(h, size);
}
