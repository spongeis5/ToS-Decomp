#include "types.h"

// sub_82639C38 -- add two floats and forward. 20 B, 10 callers.
//   lfs f0,92(r3) ; lfs f13,88(r3) ; lwz r3,48(r3) ; fadds f1,f0,f13
//   b 0x826815C0
//
// Both floats are read from r3 BEFORE r3 is overwritten with s->target, so
// nothing has to be kept in a spare register. Writing the call as
// `Emit(s->target, s->a + s->b)` made the compiler stash `s` in r11 first
// and cost an extra instruction; computing the sum into a local first
// removes the need.
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
