#include "types.h"

// sub_82639C38 -- add two floats and forward. 20 B, 10 callers.
//   lfs f0,92(r3) ; lfs f13,88(r3) ; lwz r3,48(r3) ; fadds f1,f0,f13
//   b 0x826815C0
//
// MATCHED at /O2. The answer was a PARAMETER THAT IS NEVER MENTIONED IN THE
// BODY, and it was readable off the CALLEE rather than off this function.
//
// What was wrong before: the file declared `void Emit(void*, float)` and
// `EmitSum(FSrc*)`, which compiles to 24 bytes --
//
//      mr r11,r3 ; lfs f0,92(r3) ; lwz r3,48(r3) ; lfs f13,88(r11)
//      fadds f1,f0,f13 ; b Emit
//
// -- one word right of five, with the second float load pushed past the
// clobber of r3 and paid for with a copy. Eight source spellings, seven
// tail-call-forwarding shapes and /O2 /Os were all byte-identical to that,
// and the file recorded the ordering as unreachable from the source. It is
// not; nothing about the ordering was the problem.
//
// THE EVIDENCE IS IN THE CALLEE. sub_826815C0 opens
//
//      mflr r12 ; ... ; li r10,208 ; lwz r11,8(r3)
//      lvx128 v0,r0,r4                <-- READS r4
//      mr r31,r3 ; ... ; stvx128 v0,r3,r10
//
// so it takes a 16-byte value by address in r4. sub_82639C38 never writes
// r4 and tail-calls it, so **r4 is live-in here**: this function has a
// second parameter it passes straight through and never touches. Declaring
// it is the whole fix --
//
//      void EmitSum(FSrc* s, const void* v) { Emit(s->target, v, s->a + s->b); }
//
// -- 20 bytes, 4 of 4 non-relocated words, and the `mr` is gone. With r4
// already holding an outgoing argument MSVC no longer schedules the pointer
// load into the middle of the float pair; both `lfs` issue first and `s` is
// dead before r3 is overwritten, so there is nothing to preserve.
//
// Three spellings all match identically: `const void*`, the same with the
// sum named in a local, and a `const Vec4*` with the addends written the
// other way round. The bytes therefore say the argument exists and say
// nothing about its type, so the least-claiming spelling is kept -- what the
// callee does with it (`lvx128`, 16 bytes, alignment-masked address) is
// recorded here instead of asserted as a struct.
//
// THE GENERAL LESSON, which is why the comment is this long: a register that
// a function neither writes nor reads is invisible in its own disassembly
// and is still part of its signature. Reading only the target's five
// instructions cannot find it. When a tail call will not come out and the
// residue is a copy around the argument registers, disassemble the CALLEE
// and look for a register it consumes that the caller never produces.
struct FSrc { char unk0000[0x30]; void* target; char unk0034[0x58 - 0x34];
              f32 b; f32 a; };
ASSERT_OFFSET(FSrc, target, 0x30);
ASSERT_OFFSET(FSrc, b,      0x58);
ASSERT_OFFSET(FSrc, a,      0x5C);

/* r4 is passed through untouched; sub_826815C0 loads 16 bytes from it. */
void Emit(void*, const void*, float);

void EmitSum(FSrc* s, const void* v)
{
    Emit(s->target, v, s->a + s->b);
}
