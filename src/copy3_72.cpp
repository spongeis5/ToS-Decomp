#include "types.h"

// sub_822D4118 -- copy three words through a pointer. 32 B, 6 callers.
//   lwz r11,72(r3) ; lwz r10,68(r11) ; stw r10,0(r4)
//   lwz r9,72(r11) ; stw r9,4(r4) ; lwz r8,76(r11) ; stw r8,8(r4) ; blr
// Load and store are interleaved rather than batched, which is what writing
// three separate assignments produces.
struct Src3 { char unk0000[0x44]; s32 x; s32 y; s32 z; };
struct Own72 { char unk0000[0x48]; Src3* src; };
struct Dst3 { s32 x; s32 y; s32 z; };
ASSERT_OFFSET(Src3,  x,   0x44);
ASSERT_OFFSET(Own72, src, 0x48);
void Copy3From72(Own72* o, Dst3* d)
{
    Src3* s = o->src;
    d->x = s->x; d->y = s->y; d->z = s->z;
}
