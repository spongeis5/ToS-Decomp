#include "types.h"

// sub_82663260 -- constructor. 40 bytes, 77 callers.
//
//      lis     r11,-32249
//      lis     r10,-32104
//      addi    r11,r11,-18540   ; = 8206B794, the vtable
//      addi    r10,r10,-7028    ; = 8297E48C
//      stw     r11,0(r3)        vtable
//      li      r9,0
//      li      r11,1
//      stw     r10,8(r3)
//      stw     r9,12(r3)
//      stw     r11,4(r3)
//      blr
//
// The row is recorded as 84 bytes: a second constructor for the same class
// sits at 82663290 after one word of zero padding, taking the +8 pointer as
// an argument instead of defaulting it. `match.py`'s can_shrink() proves the
// row covers two bodies and compares only this one.
//
// Store order is 0, 8, 12, 4 -- source order, not field order. The vtable
// store leads here, unlike sub_825FE880 where it will not move to the front
// in any source order tried. The difference is that here the address is
// ready two instructions earlier, because BOTH lis instructions are issued
// before either addi.
//
// 8206B794 is a vtable and 8297E48C is a table of pointers sitting directly
// in front of the RTTI string `.?AVhkpConstraint...`, so this is Havok or
// something wrapping it. That does not change how it is written.
struct VTRef;
extern const VTRef kVTable_8206B794;

struct RefTarget;
extern const RefTarget kDefaultTarget_8297E48C;

struct Ref
{
    /* 0x00 */ const VTRef*     vt;
    /* 0x04 */ s32              refCount;
    /* 0x08 */ const RefTarget* target;
    /* 0x0C */ s32              flags;
};
ASSERT_OFFSET(Ref, target, 0x08);
ASSERT_OFFSET(Ref, flags, 0x0C);

void ConstructRef(Ref* r)
{
    r->vt       = &kVTable_8206B794;
    r->target   = &kDefaultTarget_8297E48C;
    r->flags    = 0;
    r->refCount = 1;
}
