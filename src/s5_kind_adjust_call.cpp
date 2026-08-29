#include "types.h"

// sub_8224BCA0 -- dispatch on a small kind byte, tail-calling one of two
// handlers with the object adjusted by a byte offset stored in the object
// itself. 88 B, 7 callers.
//
//   lbz   r9,24(r4)      k = o->kind        (SIGNED: extsb + cmpwi)
//   mr    r10,r3         p out of r3
//   mr    r11,r4         o out of r4
//   extsb r9,r9
//   cmpwi cr6,r9,1
//   bne-  cr6,two
//   lbz   r9,16(r4)      o->adjust          (re-read inside the branch)
//   mr    r4,r3          arg2 = p
//   extsb r10,r9
//   add   r3,r10,r11     arg1 = (s8)o->adjust + (char*)o
//   b     0x826328C8
// two:
//   cmpwi cr6,r9,2
//   bnelr cr6            k is neither 1 nor 2 -- nothing to do
//   lbz   r9,16(r11)     o->adjust          (re-read again)
//   cmplwi cr6,r10,0
//   addi  r4,r10,4       \
//   extsb r9,r9           |  the BASE-CLASS UPCAST idiom, +4:
//   add   r3,r9,r11       |  null must stay null, and p is not dereferenced
//   bne-  cr6,go          |  here at all, so the test guards only the adjust
//   li    r4,0           /
// go:
//   b     0x8268D810
//   blr                  <- the unreachable blr MSVC appends after a tail call
//
// The kind byte is loaded once and compared twice (`cmpwi 1`, then `cmpwi 2`
// on the same register), which is an if/else-if on one value, not a switch.
// `extsb` before `cmpwi` says the field is a SIGNED char; an unsigned one
// would be clrlwi + cmplwi.
//
// `o->adjust` is loaded SEPARATELY in each arm rather than hoisted, so the
// source reads it inside each branch.
//
// The +4 on the second argument of the second call is the upcast idiom from
// MATCHED.md's table (`cmplwi rX,0 ; addi rY,rX,n ; bne- ; li rY,0`), so the
// two handlers take different base subobjects of the same argument -- the
// first the whole thing, the second a base at offset 4.

struct KindNode
{
    /* 0x00 */ char unk0000[0x10];
    /* 0x10 */ s8   adjust;
    /* 0x11 */ char unk0011[0x07];
    /* 0x18 */ s8   kind;
};
ASSERT_OFFSET(KindNode, adjust, 0x10);
ASSERT_OFFSET(KindNode, kind, 0x18);

struct ArgHead
{
    s32 head;
};

struct ArgBase
{
    s32 base;
};

struct ArgFull : ArgHead, ArgBase
{
};
ASSERT_OFFSET(ArgFull, base, 0x04);

struct Handled;

void HandleOne(Handled* h, ArgFull* a);
void HandleTwo(Handled* h, ArgBase* a);

void DispatchKind(ArgFull* a, KindNode* o)
{
    if (o->kind == 1)
        HandleOne((Handled*)((char*)o + o->adjust), a);
    else if (o->kind == 2)
        HandleTwo((Handled*)((char*)o + o->adjust), static_cast<ArgBase*>(a));
}
