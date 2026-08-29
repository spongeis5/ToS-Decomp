#include "types.h"

// sub_822D2978 -- dispatch through a base-class upcast. 24 B, 6 callers.
//
//      cmplwi  cr6,r3,0
//      addi    r4,r3,256
//      bne-    cr6,go
//      li      r4,0
// go:  lwz     r3,268(r3)
//      b       0x8261AD60
//
// The null test guards ONLY the +256 adjustment: r3 is dereferenced at +268
// regardless, so a null pointer would fault anyway. That rules out a
// hand-written `p ? &p->sub : 0` -- nobody writes a null check that the very
// next instruction ignores.
//
// It is a BASE-CLASS UPCAST. `static_cast<Tail*>(n)` where Tail sits at
// offset 256 must yield null for a null n, so MSVC emits exactly this:
// adjust, and substitute zero if the original was null. The compiler does
// not know n is non-null; the programmer did.
struct Head
{
    char unk0000[256];
};

struct Tail
{
    s32   f00;
    s32   f04;
    s32   f08;
    void* handler;
};
ASSERT_OFFSET(Tail, handler, 12);

struct Node : Head, Tail
{
};
ASSERT_OFFSET(Node, handler, 268);

void Dispatch(void* handler, Tail* t);

void GoNode(Node* n)
{
    Dispatch(n->handler, static_cast<Tail*>(n));
}
