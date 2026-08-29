#include "types.h"

// sub_822D40F8 -- the twin of sub_822D4118, reading the pointer from +68
// instead of +72. 32 B, 6 callers. They are 32 bytes apart, so adjacent.
struct Src3b { char unk0000[0x44]; s32 x; s32 y; s32 z; };
struct Own68 { char unk0000[0x44]; Src3b* src; };
struct Dst3b { s32 x; s32 y; s32 z; };
ASSERT_OFFSET(Src3b, x,   0x44);
ASSERT_OFFSET(Own68, src, 0x44);
void Copy3From68(Own68* o, Dst3b* d)
{
    Src3b* s = o->src;
    d->x = s->x; d->y = s->y; d->z = s->z;
}
