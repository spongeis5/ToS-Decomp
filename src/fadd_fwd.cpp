#include "types.h"

// sub_82639C38 -- add two floats and forward. 20 B, 10 callers.
//   lfs f0,92(r3) ; lfs f13,88(r3) ; lwz r3,48(r3) ; fadds f1,f0,f13
//   b 0x826815C0
//
// Both floats are read from r3 BEFORE r3 is overwritten with s->target, so
// nothing has to be kept in a spare register.
//
// NOT MATCHED: 1 of 5 words, 24 bytes against 20. We emit `mr r11,r3` first
// and then interleave `lwz r3,48(r3)` BETWEEN the two `lfs`, so the second
// float has to come from the copy. The target issues both `lfs` first and
// needs no copy at all.
//
// The claim this file used to make -- that computing the sum into a local
// removes the copy -- is WRONG, and it was wrong when written. Eight
// spellings were compiled and every one is byte-identical, `mr` included:
// the sum in a local and the target in the call; both floats in locals and
// the target in the call; all three named in the target's own read order;
// nothing named at all; a const view for the target read; a const view for
// the float reads; and the member-function form.
//
// So the pointer load is hoisted above the second float load no matter where
// the source puts it. MSVC schedules the argument register's producer early
// because it is the one value the tail call cannot start without, and pays
// the `mr` to do it. Nothing at the source level was found that changes
// that ordering, and /O2 /Os does not either.
//
// THE LEVER THAT SOLVED THE OTHER SHUFFLE DOES NOT WORK HERE, and that is
// worth recording because the two functions look like the same problem.
// sub_8215E5B0 is also a register shuffle in front of a tail call, also
// carried a wrong `mr`, and came out by FORWARDING the callee's result --
// which keeps r3 live out and changes what the copy sequencer stages. Seven
// forwarding shapes were compiled here at both levels and every one is
// byte-identical to the void baseline, `mr r11,r3` included:
//
//     return void* / int / float / bool from the tail call
//     the sum written inline in the call rather than named
//     the whole thing as a member function returning the result
//     a non-virtual member call on `s->target` whose result is returned
//
// The difference is that sub_8215E5B0's shuffle is a copy CYCLE -- r3 gets
// r5's value while r4 gets a load through r3 -- so something must be staged
// and the return type decides what. Here there is no cycle: one float load
// simply has to survive r3 being overwritten, and MSVC always solves that
// the same way. 1 of 5 words, 24 bytes against 20.
struct FSrc { char unk0000[0x30]; void* target; char unk0034[0x58 - 0x34];
              f32 b; f32 a; };
ASSERT_OFFSET(FSrc, target, 0x30);
ASSERT_OFFSET(FSrc, b,      0x58);
ASSERT_OFFSET(FSrc, a,      0x5C);
void Emit(void*, float);

void EmitSum(FSrc* s)
{
    float sum = s->a + s->b;
    Emit(s->target, sum);
}
