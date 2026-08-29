#include "types.h"

// sub_827FE8A0 -- return a cached pointer, or build one. 36 B, 12 callers.
//
//      mr      r11,r3          keep `this`; r3 is wanted for the tail call
//      mr      r3,r4           ctx moves into the first argument slot early
//      lwz     r10,4(r11)
//      cmplwi  cr6,r10,0
//      beq-    cr6,make
//      mr      r3,r10          <- a materialise-through-copy: a NAMED local
//      blr
// make:addi    r4,r11,12
//      b       0x827D99C0
//
// Two things fix the shape. The `beq-` jumps AWAY to the tail call, so the
// `return cached` path is the fall-through and has to be written FIRST. And
// the `mr r3,r10` is the fingerprint of a named local -- written inline as
// `if (o->cached) return o->cached;` the compiler loads straight into r3 and
// the copy disappears.
struct Cached
{
    char  unk0000[4];
    void* cached;
    char  unk0008[4];
    void* slot;
};
ASSERT_OFFSET(Cached, cached, 0x04);
ASSERT_OFFSET(Cached, slot, 0x0C);

void* MakeInto(void* ctx, void* slot);

void* GetCached(Cached* o, void* ctx)
{
    void* v = o->cached;
    if (v)
        return v;
    return MakeInto(ctx, &o->slot);
}
