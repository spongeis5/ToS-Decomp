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
    text   = s;
    length = s ? (u32)strlen(s) : 0;
}
