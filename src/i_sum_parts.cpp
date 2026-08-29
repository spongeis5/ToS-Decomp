#include "types.h"

// sub_821675B8 -- walk two pointers down and add two fields. 56 B, 13 callers.
//
//      addic.  r11,r3,208       compute r3 + 208 AND test it
//      beq-    zero
//      lwz     r11,232(r3)
//      lwz     r11,104(r11)
//      cmplwi  cr6,r11,0
//      beq-    cr6,zero
//      addic.  r11,r11,24       compute r11 + 24 AND test it
//      beq-    zero
//      lwz     r10,44(r11)
//      lwz     r11,32(r11)
//      add     r3,r10,r11
//      blr
// zero:li      r3,0
//      blr
//
// The two `addic.`s are the thing to read carefully. They are NOT the
// null-preserving base upcast idiom -- that one tests the ORIGINAL
// (`cmplwi rX,0 ; addi rY,rX,n ; bne- ; li rY,0`). Here the SUM is what gets
// tested, and in the first case the sum is then thrown away: the next
// instruction goes back to r3. So the source computes an interior address,
// asks whether it is null, and does not otherwise use it.
//
// add rA is the operand whose source read comes later, so the fields go
// lo (+32) then hi (+44) even though the loads are scheduled the other way
// round -- the base register is clobbered by the second load.
//
// BRANCH POLARITY DECIDED THIS ONE, AND IT SCORED 2 OF 14 THE OTHER WAY --
// with three guards, not the one word the MATCHED.md note describes. Written
// as three flat `if (x == 0) return 0;` guards, MSVC emits the shared `li
// r3,0 ; blr` IMMEDIATELY AFTER THE FIRST TEST, inverts that test to `bne-`
// to jump over it, and makes the other two guards branch BACKWARD into it.
// Same instructions, same 56 bytes, twelve words displaced. Nesting the
// positive path and putting the single `return 0` last is 14 of 14.
//
// So the lever scales: on a function with ONE guard, getting the polarity
// backwards costs a word; on a function whose guards SHARE an exit block, it
// relocates the block and costs nearly the whole function. The tell is which
// DIRECTION the guards branch -- all forward to a common tail means the
// failure path is written last.
//
// Flag-insensitive: identical bytes at /O2 and at /O2 /Os, so this one
// carries no evidence about the translation unit either way.
struct PartLeaf
{
    /* 0x00 */ char unk0000[0x20];
    /* 0x20 */ s32  lo;
    /* 0x24 */ char unk0024[8];
    /* 0x2C */ s32  hi;
};
ASSERT_OFFSET(PartLeaf, lo, 0x20);
ASSERT_OFFSET(PartLeaf, hi, 0x2C);

struct PartMid
{
    /* 0x00 */ char     unk0000[0x18];
    /* 0x18 */ PartLeaf leaf;
};
ASSERT_OFFSET(PartMid, leaf, 0x18);

struct PartOwner
{
    /* 0x00 */ char     unk0000[0x68];
    /* 0x68 */ PartMid* mid;
};
ASSERT_OFFSET(PartOwner, mid, 0x68);

struct PartGuard
{
    /* 0x00 */ char unk0000[0x18];
};

struct PartRoot
{
    /* 0x000 */ char       unk0000[0xD0];
    /* 0x0D0 */ PartGuard  guard;
    /* 0x0E8 */ PartOwner* owner;
};
ASSERT_OFFSET(PartRoot, guard, 0xD0);
ASSERT_OFFSET(PartRoot, owner, 0xE8);

s32 SumParts(PartRoot* r)
{
    PartGuard* g = &r->guard;
    if (g != 0)
    {
        PartMid* m = r->owner->mid;
        if (m != 0)
        {
            PartLeaf* p = &m->leaf;
            if (p != 0)
                return p->lo + p->hi;
        }
    }
    return 0;
}
