// sub_826A3648 -- point a slot at a static shared object and take a reference
// on it. 52 B, 75 callers.  /O2 /Os
//
//   lis    r11,-32104
//   li     r7,1
//   addi   r11,r11,2440        r11 = &g_shared          (0x82980988)
//   stw    r11,0(r3)           this->data = &g_shared
//   addi   r11,r11,4           r11 = &g_shared.refCount (0x8298098C)
// L:mfmsr  r8
//   mtmsrd r13,1
//   lwarx  r10,0,r11
//   add    r9,r7,r10
//   stwcx. r9,0,r11
//   mtmsrd r8,1
//   bne+   L
//   blr
//
// `addi r11,r11,4` REUSES the address just stored rather than materialising a
// second lis/addi pair, so both addresses are the same relocated symbol at two
// offsets -- one object with a field at +4, not two globals four bytes apart.
// Two unrelated globals could not share a register: each would carry its own
// relocation.
//
// The mfmsr / mtmsrd r13,1 ... mtmsrd r8,1 bracket around the lwarx/stwcx.
// pair is the XDK compiler's own expansion of the interlocked intrinsics on
// Xenon; the source does not spell it out. It is _InterlockedExchangeAdd and
// NOT _InterlockedIncrement: the increment intrinsic folds the constant into
// the reservation loop as `addi r10,r10,1` and needs no extra register, while
// this materialises 1 into r7 and uses a full `add`. That one instruction is
// the difference between a 36-byte body and this 40-byte one. Same reasoning
// as sub_826632F8.
//
// TWO THINGS HAD TO BE RIGHT, and one of them is a lever worth keeping.
//
// 1. IT IS A CONSTRUCTOR. Written as a free function -- or as an ordinary
//    member function returning void -- the compiler hoists the address
//    computation ABOVE the store:
//
//        addi   r6,r11,4
//        stw    r11,0(r3)
//
//    and because the base is then still live at the addi, the addi gets a
//    fresh register and drags it through the lwarx and the stwcx.: four wrong
//    words instead of none. No source rearrangement moves it. Eleven shapes
//    were tried -- the store first, a local for the pointer, `p += 4` on one
//    variable, a volatile slot, a volatile member, a volatile refcount, the
//    address as an array element, a cast-free `long[2]` view, an inlined
//    member AddRef, the increment reached through the just-stored pointer,
//    and a plain member function -- and all eleven emitted the same two
//    instructions in the same wrong order.
//
//    A constructor reorders them. So does `operator=`. What those two have in
//    common is that r3 is live OUT of the function: MSVC returns `this` from
//    both. A `void` member function of the same class does not reorder.
//
//    So: WHEN A STORE TO *this MUST PRECEDE AN ADDRESS COMPUTATION AND THE
//    COMPILER INSISTS ON HOISTING THE ADDRESS, TRY A FORM WHOSE RETURN VALUE
//    IS `this`. That is a lever alongside the member-function one, and it is
//    a different lever: making it a member is not enough, the return has to
//    be `this`.
//
// 2. /Os, not /O2. The constructor at /O2 still allocates r6 for the +4
//    address and differs in three words; at /Os the same source coalesces it
//    onto r11 and every non-relocated word is identical. Neither change alone
//    matches -- the free function at /Os scores 7 of 13, the constructor at
//    /O2 scores 8.

#include "types.h"

extern "C" long __cdecl _InterlockedExchangeAdd(long volatile* addend, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

struct SharedData
{
    u32  unk0000;
    long refCount;
};
ASSERT_OFFSET(SharedData, refCount, 0x04);

extern SharedData g_sharedEmpty;

struct SharedRef
{
    SharedData* data;
    SharedRef();
};

SharedRef::SharedRef()
{
    data = &g_sharedEmpty;
    _InterlockedExchangeAdd(&g_sharedEmpty.refCount, 1);
}
