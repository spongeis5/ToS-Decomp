#include "types.h"

// sub_826731B0 -- initialise an object: vtable, three small integers and one
// float from the argument. 40 B, 20 callers.
//
//   lis  r10,-32249
//   stfs f1,16(r3)
//   li   r11,1
//   addi r9,r10,-17780        -> 8206BA8C, a vtable (16 code pointers,
//   li   r8,0                    sub_826731D8 -- the function that follows
//   sth  r11,6(r3)               this one -- is slot 8)
//   stw  r9,0(r3)
//   stw  r8,8(r3)
//   stw  r11,12(r3)
//   blr
//
// The emitted store order is 16, 6, 0, 8, 12, which looks like it must be
// pinning down a source order. It is not: all five orderings tried -- the
// emitted one, vtable-first, float-last, and two others -- give the same
// eight words. MSVC schedules the cheap `stfs` and `sth` into the slots
// where the `lis`/`addi` pair for the vtable address is still in flight, so
// the store order here is the SCHEDULER's, and carries no information about
// the source. Written vtable-first because that is what a constructor emits.
//
// Worth keeping next to the store-order rule in MATCHED.md: store order is
// source order when the stores need no address computation between them.
// Here one of them does, and the scheduler fills the gap.

struct VTable;
extern const VTable kVTable_8206BA8C;

struct Obj1
{
    /* 0x00 */ const VTable* vt;
    /* 0x04 */ char          unk0004[0x02];
    /* 0x06 */ s16           a;
    /* 0x08 */ s32           b;
    /* 0x0C */ s32           c;
    /* 0x10 */ f32           v;
};
ASSERT_OFFSET(Obj1, vt, 0x00);
ASSERT_OFFSET(Obj1, a,  0x06);
ASSERT_OFFSET(Obj1, b,  0x08);
ASSERT_OFFSET(Obj1, c,  0x0C);
ASSERT_OFFSET(Obj1, v,  0x10);

void InitObj1(Obj1* o, float value)
{
    o->vt = &kVTable_8206BA8C;
    o->a  = 1;
    o->b  = 0;
    o->c  = 1;
    o->v  = value;
}
