#include "types.h"

// sub_82543F60 -- load, guard, tail-call, else zero. 24 B, 5 callers.
//   lwz r3,280(r3) ; cmplwi cr6,r3,0 ; beq- cr6,0x82543F70
//   b 0x82540658 ; li r3,0 ; blr
//
// `beq-` jumps AWAY to the zero return, so the tail call is the fall-through
// and the compiler expected the pointer to be non-null. Writing the guard as
// `if (!p) return 0;` produced the opposite polarity; writing the positive
// path first reproduces it.
struct Own280 { char unk0000[0x118]; void* obj; };
ASSERT_OFFSET(Own280, obj, 0x118);
void* Query(void*);

void* QueryOrNull(Own280* o)
{
    void* p = o->obj;
    if (p)
        return Query(p);
    return 0;
}
