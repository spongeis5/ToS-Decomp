// sub_82700B30 -- constructor of a class with TWO non-virtual bases: two
// vtable pointers and one pointer field are written by the bases and again
// by the derived part, five bitfields are set, and a 32-bit argument is
// widened to 64 bits. 176 B, 9 callers.  r3 = this, r4 = a pointer, r5 = int.
//
// NOT A MATCH: 9 of 44 words at /O2, correct LENGTH (176 bytes) and correct
// instruction multiset. What is left is scheduling and register allocation
// inside a 44-instruction constructor -- see the measurements at the bottom.
//
// THE FIVE `rlwimi`s DECODE EXACTLY, and each is the FIRST-declared (high)
// field of its storage unit, which is what MSVC's MSB-first allocation
// gives. Reading `rlwimi rA,rS,SH,MB,ME` as
// `rA = (ROTL(rS,SH) & mask(MB,ME)) | (rA & ~mask(MB,ME))`:
//
//   rlwimi r8,r9,22,0,9    keep 003FFFFF   [16] bits 31..22 <- 33
//   rlwimi r31,r10,7,0,27  keep 0000000F   [22] bits  7..4  <- 8
//   rlwimi r6,r10,6,0,30   keep 00000001   [20] bits  7..1  <- 32
//   rlwimi r6,r10,7,0,25   keep 0000003F   [32] bits  7..6  <- 2
//   rlwimi r31,r7,28,0,4   keep 07FFFFFF   [28] bits 31..27 <- 10
//
// The kept mask names the field's low bit: keep = (1 << P) - 1, and ME is
// 31 - P, so P and the width follow from the two together. The SHIFT does
// NOT equal P, and that is the readable part: MSVC materialises each
// constant as its ODD part and folds the power of two into the rotate, then
// CSEs the odd parts. One `li r10,1` serves 8 (SH 4+3), 32 (SH 1+5) and 2
// (SH 6+1); `li r7,5` serves 10 (SH 27+1); 33, being odd, is materialised as
// itself with SH = P = 22. Every one of the five reproduces here.
//
// THE DUPLICATE STORES ARE THE SHAPE, and they are what a flat function
// cannot express. [0], [12] and [24] are each written twice with different
// values and nothing reads them in between. Written as one function with
// plain pointer fields, MSVC's dead-store elimination removes all three
// first writes: 144 bytes against 176, four callee-saved registers spilled
// and a `bl __savegpr` prologue the target does not have. Only a real base
// subobject keeps them -- a vptr assignment inside a constructor is not
// dead, because C++ says the object is of the base's type while that
// constructor runs. Two bases of 12 and 16 bytes, the second's vptr at
// offset 12, is exactly what the two `lis`/`addi` pairs into 0(r3) and
// 12(r3) say, and it restores the length to 176 exactly.
//
// WHAT IS LEFT, measured:
//
//   two bases, flat BaseB                       176 B,  9 of 44   <- this
//   two bases, f22 before f20 / f32 before f28  176 B,  8 of 44
//   Flags subobject with a two-level ctor       188 B,  0 of 44
//   three bases, BaseC at 16                    184 B,  1 of 44
//   flat function, plain pointer fields         144 B   (DSE, above)
//
// The remaining differences are where the second base's vptr store sits
// (82700B94 in the target, hoisted to 82700B6C here) and which registers
// carry the four read-modify-write temporaries. One diagnostic is worth
// recording for whoever picks this up: the DEAD `addi r10,r3,16` at
// 82700BC4 is the inlined-subobject fingerprint of src/t7_ctor_args5.cpp,
// and this source leaves `addi r10,r3,12` in the same place -- the second
// base's own address. So the sub-object whose constructor is inlined starts
// at 16, not at 12, and the layout that puts it there is the thing still to
// find. The two arrangements tried (a Flags member with its own two-level
// constructor, and a third base at 16) both raise register pressure enough
// to spill r30 and are further away, not nearer.

#include "types.h"

struct VTa;
struct VTb;

struct BaseA
{
    virtual void FnA();

    /* 0x04 */ void* f04;
    /* 0x08 */ u8    f08;

    BaseA(void* a) { f04 = a; f08 = 0; }
};

struct BaseB
{
    virtual void FnB();

    /* 0x10 */ u32 f16a : 10;
    /* 0x10 */ u32 f16b : 22;
    /* 0x14 */ u8  f20a : 7;
    /* 0x14 */ u8  f20b : 1;
    /* 0x15 */ u8  f21;
    /* 0x16 */ u8  f22a : 4;
    /* 0x16 */ u8  f22b : 4;
    /* 0x17 */ u8  unk0017;
    /* 0x18 */ u8* f24;

    BaseB()
    {
        f16a = 33;
        f21 = 0;
        f20a = 32;
        f22a = 8;
        f24 = 0;
    }
};

struct Obj : BaseA, BaseB
{
    virtual void FnA();
    virtual void FnB();

    /* 0x1C */ u32 f28a : 5;
    /* 0x1C */ u32 f28b : 27;
    /* 0x20 */ u8  f32a : 2;
    /* 0x20 */ u8  f32b : 6;
    /* 0x21 */ u8  unk0021[7];
    /* 0x28 */ s64 f40;
    /* 0x30 */ u8  unk0030[0x1C];
    /* 0x4C */ u8  f76;

    Obj(void* a, int n);
};

Obj::Obj(void* a, int n)
    : BaseA(a)
    , BaseB()
{
    f24 = &f76;
    f28a = 10;
    f32a = 2;
    f40 = n;
    f76 = 0;
}
