#include "types.h"

// sub_827007F8 -- constructor: vtable, owner, a zero byte, a string and its
// measured length. 76 B, 6 callers.
//
//      lis     r10,-32248
//      stw     r4,4(r3)          +0x04 = owner
//      li      r11,0
//      addi    r10,r10,-11752    = 0x8207D218, a vtable
//      stb     r11,8(r3)         +0x08 = 0  (a BYTE)
//      cmplwi  cr6,r5,0
//      stw     r10,0(r3)         +0x00 = the vtable
//      stw     r5,12(r3)         +0x0C = text
//      beq-    cr6,skip
//      mr      r11,r5
//  L:  lbz     r10,0(r11)
//      addi    r11,r11,1
//      cmplwi  cr6,r10,0
//      bne+    cr6,L
//      subf    r11,r5,r11
//      addi    r11,r11,-1
//      rotlwi  r11,r11,0         the 32->64 zero extension of size_t
// skip:stw     r11,16(r3)        +0x10 = length
//      blr
//
// Two things fix this one.
//
// 1. THE STRLEN IS THE INTRINSIC, not a hand-written loop. The trailing
//    `rlwinm r11,r11,0,0,31` is the tell recorded in MATCHED.md and in
//    src/b_setstr_len.cpp: a hand-written walk folds the -1 into the
//    destination register instead and never materialises the unsigned width.
//
// 2. THE VTABLE STORE LANDS THIRD despite being written first. Its address is
//    still in flight through the lis/addi pair and the scheduler fills the gap
//    with the two cheap independent stores -- the same exception to
//    "store order is source order" that sub_826731B0 and src/m_ctor_7zero.cpp
//    record.
//
// The zero shared between `stb` and the null-string case is one `li r11,0`,
// which is what makes the length a conditional expression rather than two
// separate assignments.
//
// FLAGS: this is the immediate neighbour of sub_827007E8
// (src/set_vtable_827007E8.cpp, /O2 /Os) -- 16 bytes earlier, storing a vtable
// 12 bytes away in the same table. That is a genuine adjacency, unlike the
// 8.5 KB "neighbours" MATCHED.md records as a bad inference.
//
// AND THE FLAG IS CONFIRMED, WHICH THIS FILE DID NOT SAY. At /O2 the body is
// 72 bytes and has NO `rotlwi r11,r11,0` at all -- the 32-to-64 zero
// extension that is the strlen-intrinsic tell simply is not emitted -- so
// the size is one word short and nine of eighteen words are wrong. At
// /O2 /Os it is 76 bytes, the rotlwi is there, and 13 of 17 words are right.
// Measure this one at /Os or the diff is describing a different function.
//
// MATCHED at /O2 /Os, and the answer is THE ADDRESS-OF-MEMBER LEVER.
//
// What was left was the position of one store and nothing else:
//
//      target  lis, stw r4,4, li r11,0, addi r10, stb r11,8, cmplwi,
//              stw r10,0, stw r5,12, beq-
//      ours    lis, stw r4,4, li r11,0, stw r5,12, addi r10, stb r11,8,
//              cmplwi, stw r10,0, beq-
//
// `stw r5,12(r3)` -- the text store -- was issued four slots early, into the
// gap between `li r11,0` and the `addi` that finishes the vtable address.
// Writing it through a pointer TO the member puts it back where the target
// has it, 17 of 17:
//
//      const char** pt = &text;
//      *pt = s;
//
// This is MATCHED.md's sub_827FEE48 lever used on a STORE rather than on a
// load. Two constant offsets off one base provably cannot alias, so MSVC is
// free to slide the text store into the scheduling gap; taking the member's
// address removes that proof and the store stays where it was written. The
// same change on the VTABLE member instead does nothing (13 of 17), so it is
// the moved store that has to be pinned, not the one it moved past.
//
// AND IT MUST BE A BARE LOCAL POINTER: a `static void SetPtr(const char**,
// const char*)` helper called with `&text` is 13 of 17, byte-identical to
// the baseline. That is the OPPOSITE of sub_825FAC00, where eleven bare
// `int*` spellings failed and only an inlined `Pack(int*, int, int)` helper
// reached the match. So the lever has two forms and which one works is not
// predictable from the shape -- try both.
//
// STORE ORDER IS STILL NOT SOURCE ORDER HERE. All 24 permutations of the
// four stores were compiled at /Os and produce exactly TWO schedules -- the
// one above whenever `owner` is written before `text`, and the same thing
// with those two swapped otherwise. Eight structural shapes give the same:
// the length as an if/else, as a zero-initialised accumulator declared first
// and declared last, computed before the stores (84 bytes, 1 of 17 -- much
// worse), the text stored from a local copy of `s`, the vtable address named
// in a local, and a real C++ constructor with the vptr emitted by the
// compiler. A real constructor WITH the address-of lever also reaches 17 of
// 17, so the member-function-versus-constructor axis carries nothing here
// and the pointer carries all of it.
//
extern "C" size_t strlen(const char*);
#pragma intrinsic(strlen)

struct VT827007F8;
extern const VT827007F8 kVTable_8207D218;

struct MsgObject
{
    /* 0x00 */ const VT827007F8* vt;
    /* 0x04 */ void*             owner;
    /* 0x08 */ u8                flag;
    /* 0x0C */ const char*       text;
    /* 0x10 */ u32               length;

    void Init(void* o, const char* s);
};
ASSERT_OFFSET(MsgObject, owner,  0x04);
ASSERT_OFFSET(MsgObject, flag,   0x08);
ASSERT_OFFSET(MsgObject, text,   0x0C);
ASSERT_OFFSET(MsgObject, length, 0x10);

void MsgObject::Init(void* o, const char* s)
{
    owner  = o;
    flag   = 0;
    vt     = &kVTable_8207D218;

    const char** pt = &text;
    *pt = s;

    length = s ? (u32)strlen(s) : 0;
}
