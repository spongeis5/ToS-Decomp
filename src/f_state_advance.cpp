// sub_827156B8 -- if the object is in state 1, move it to state 0 or state 2
// depending on whether a 28-bit counter is still non-zero, then TAIL CALL the
// first virtual slot with a code saying which. 88 B, 20 callers.
//
// FLAGS: /O2 /Os. At plain /O2 this same source is 92 bytes and 5 of 22
// words: MSVC splits the counter test into `clrlwi` + `cmplwi cr6` + `beq-
// cr6` where the target folds it into the record form `clrlwi.` and branches
// on CR0, and the extra word shifts every register after it. /Os is what
// picks the record form, so this translation unit is one of the /Os ones.
//
//      lwz     r11,8(r3)
//      lis     r10,4096                0x10000000
//      rlwinm  r9,r11,0,1,3            & 0x70000000
//      cmplw   cr6,r9,r10
//      bnelr   cr6                     state != 1: nothing to do
//      clrlwi. r10,r11,4               & 0x0FFFFFFF, sets CR0
//      lwz     r10,0(r3)               vtable, hoisted above the branch
//      beq-    0x827156f0
//      rlwinm  r11,r11,0,4,0           clear bits 1..3       state = 0
//      li      r4,2
//      stw     r11,8(r3)
//      lwz     r11,0(r10)
//      mtctr   r11
//      bctr
//  L:  li      r9,1
//      li      r4,4
//      rlwimi  r11,r9,29,1,3           insert 1<<1 into bits 1..3  state = 2
//      stw     r11,8(r3)
//      lwz     r11,0(r10)
//      mtctr   r11
//      bctr
//      blr                             unreachable, appended after a tail call
//
// The three rlwinm/rlwimi forms are all one word at offset 8 carved into
// bitfields, MSB first as this compiler allocates them big-endian:
//   bit 0      one spare flag
//   bits 1..3  a three-value state, compared by MASK against 1<<28 rather
//              than extracted -- the signature of an unsigned bitfield
//              compared with a constant
//   bits 4..31 a counter, tested with clrlwi. in one instruction
//
// `beq-` jumps AWAY to the state-2 path, so the counter-non-zero case is the
// fall-through and has to be written FIRST.

#include "types.h"

struct Obj;

struct ObjVtbl
{
    /* 0x00 */ void (*notify)(Obj*, int);
};

struct Obj
{
    /* 0x00 */ ObjVtbl* vt;
    /* 0x04 */ u32      unk0004;
    /* 0x08 */ u32      flag  : 1;
               u32      state : 3;
               u32      count : 28;
};

ASSERT_OFFSET(Obj, unk0004, 0x04);

void ObjAdvance(Obj* o)
{
    if (o->state != 1)
        return;

    if (o->count != 0)
    {
        o->state = 0;
        o->vt->notify(o, 2);
        return;
    }

    o->state = 2;
    o->vt->notify(o, 4);
}
