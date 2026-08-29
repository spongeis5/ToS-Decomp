#include "types.h"

// sub_821C77A8 -- the stride-12 twin of sub_8228AF08. 28 B, 7 callers.
//
//      rlwinm  r11,r5,1,0,30    n*2
//      mr      r6,r4
//      add     r11,r5,r11       n*3
//      li      r5,0
//      rlwinm  r4,r11,2,0,29    *4  -> n*12
//      li      r3,0
//      b       0x82183890
//
// Same callee and the same argument rotation as m_enqueue4.cpp, with 12 in
// place of 4 -- built as (n + n*2) * 4, the shape stride24.cpp documents.
// Two element sizes, one submit routine.
struct Queue12
{
    void Enqueue(void* item, int count);
};

void Submit(int kind, int bytes, int flags, void* item);

void Queue12::Enqueue(void* item, int count)
{
    Submit(0, count * 12, 0, item);
}
