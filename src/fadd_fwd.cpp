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
