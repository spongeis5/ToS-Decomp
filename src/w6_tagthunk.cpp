#include "types.h"

// sub_825FA618 -- back up over a size byte, then either tail-jump through
// the object's stored member pointer or tail-call the deleter.
// 56 B, 4 callers.
//
//   if (p == 0) return                     (beqlr -- r3 passes through)
//   if (p[-2] == 3):
//       fn = *(Fn*)(p-12); size = p[-1]; return fn(p - size)
//       -- a TAIL call through the pointer, so mtctr/bctr not bctrl
//   size = p[-1]; return Tail_828AD8C8(p - size)

typedef int (*Fn)(void*);

int Tail_828AD8C8(void*);

int UnwindThunk(void* p)
{
    if (p == 0)
        return 0;
    if (*((unsigned char*)p - 2) == 3)
    {
        Fn fn = *(Fn*)((char*)p - 12);
        unsigned char size = *((unsigned char*)p - 1);
        return fn((char*)p - size);
    }
    unsigned char size = *((unsigned char*)p - 1);
    return Tail_828AD8C8((char*)p - size);
}

// NEAR-MISS. indirect tail call through stored fn ptr; MSVC form differs.
