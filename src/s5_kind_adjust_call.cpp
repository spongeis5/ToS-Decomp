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
//
// MATCHED, and the answer is ONE NAMED LOCAL PER ARM. Written with the
// adjusted pointer as an argument expression -- `HandleOne((Handled*)((char*)o
// + o->adjust), a)` -- the body is 84 bytes and 0 of 19 words; naming it
//
//     Handled* h = (Handled*)((char*)o + o->adjust);
//     HandleOne(h, a);
//
// in EACH arm is 88 bytes and 20 of 20, at either optimisation level.
//
// WHAT THE NAME BUYS is the `mr r10,r3` at the top -- the fourth
// instruction, and the whole word the short version is missing. Written as
// an argument, the adjusted pointer is evaluated as late as MSVC likes
// (arguments go right to left, so the upcast is materialised first) and the
// second arm becomes upcast-then-adjust: `cmplwi ; addi r4,r3,4 ; bne- ;
// li r4,0` with `a` still in r3, so no copy of r3 is ever needed. Named, the
// adjustment is computed FIRST in each arm, which puts `add r3,...` ahead of
// the upcast's branch -- and once r3 is written there, `a` has to be saved
// somewhere before the branch, which is the copy. The dead-looking `mr` at
// the top is therefore a consequence of a statement order two branches away.
//
// This is MATCHED.md's "naming a sub-expression as a local" lever
// (sub_82154A68), and it is the second time in this batch that it decided
// whether a value is computed early or late rather than which register it
// lands in.
//
// Seven other shapes were measured, all 84 bytes (or 80) and all 0 of 19,
// which is what makes the naming the answer rather than one option: the kind
// byte in a local; the two arms as a `switch` (80 bytes -- MSVC builds a
// different compare chain); two flat `if`s with a `return` between them; the
// adjust BYTE in a local instead of the whole pointer; both parameters
// copied into locals up front; and the second call's two arguments named in
// the emitted order with the upcast first. Naming the upcast as well as the
// pointer is also 20 of 20, so the upcast's name carries nothing -- only the
// adjusted pointer's does.

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
    {
        Handled* h = (Handled*)((char*)o + o->adjust);
        HandleOne(h, a);
    }
    else if (o->kind == 2)
    {
        Handled* h = (Handled*)((char*)o + o->adjust);
        HandleTwo(h, static_cast<ArgBase*>(a));
    }
}
