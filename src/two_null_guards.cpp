#include "types.h"

// sub_82917B88 -- two null guards then a tail call. 28 B, 7 callers.
//   cmplwi cr6,r4,0 ; beqlr cr6 ; lwz r3,4(r4)
//   cmplwi cr6,r3,0 ; beqlr cr6 ; b 0x828BDFD8 ; blr
// The first argument is never used; the second is tested, dereferenced, and
// its field tested again before becoming the call's only argument.
struct Holder4 { char unk0000[0x04]; void* obj; };
ASSERT_OFFSET(Holder4, obj, 0x04);
void Consume(void*);
void ConsumeIfBoth(void*, Holder4* h)
{
    if (!h)
        return;
    void* o = h->obj;
    if (!o)
        return;
    Consume(o);
}
