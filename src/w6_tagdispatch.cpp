#include "types.h"

// sub_82807180 -- dispatch on a tag byte with two tail calls. 68 B, 3
// callers.
//
//   tag = *r3
//   if (tag == 8) { r3 += 4; Tail_828125A0(r3); }
//   if (tag == 6) { p = *(r3+4); if (p) Tail_827156B8(p); }
//   if (tag != 9) return
//   p = *(r3+4); Tail_827156B8(p)

int Tail_828125A0(void*);
int Tail_827156B8(void*);

int Dispatch(void* obj)
{
    unsigned char tag = *(unsigned char*)obj;
    if (tag == 8)
        return Tail_828125A0((char*)obj + 4);
    if (tag == 6)
    {
        void* p = *(void**)((char*)obj + 4);
        if (p != 0)
            return Tail_827156B8(p);
    }
    if (tag != 9)
        return 0;
    return Tail_827156B8(*(void**)((char*)obj + 4));
}

// NEAR-MISS. tag chain; tail-call grouping differs.
