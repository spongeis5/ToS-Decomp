#include "types.h"

// sub_82272AA0 -- follow the first child, or fall back to a sub-object.
// 28 B, 6 callers.
//
//      lwz     r11,0(r3)
//      cmplwi  cr6,r11,0
//      beq-    cr6,self
//      lwz     r3,64(r11)
//      b       0x82272A98
// self:addi    r3,r3,4
//      b       0x8225CF80
//
// Two DIFFERENT tail calls, which is why this is not a null-guard around one
// call. The `beq-` jumps away to the second, so the first is the
// fall-through and has to be written first.
struct Child
{
    char  unk0000[64];
    void* payload;
};
ASSERT_OFFSET(Child, payload, 64);

struct Parent
{
    Child* first;
    char   sub[4];
};
ASSERT_OFFSET(Parent, sub, 4);

int UsePayload(void* payload);
int UseSelf(void* sub);

int Resolve(Parent* p)
{
    Child* c = p->first;
    if (c)
        return UsePayload(c->payload);
    return UseSelf(p->sub);
}
