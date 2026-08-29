#include "types.h"

// sub_8228AF08 -- scale a count and hand it on. 24 B, 12 callers.
//
//      mr      r11,r5
//      mr      r6,r4
//      li      r5,0
//      rlwinm  r4,r11,2,0,29    n * 4
//      li      r3,0
//      b       0x82183890
//
// r3 comes in and is never read, only overwritten with 0. An unused first
// parameter that is not even loaded is `this` -- a free function with a dead
// first argument would be odd source, a member function that happens not to
// touch its object is ordinary.
struct Queue
{
    void Enqueue(void* item, int count);
};

void Submit(int kind, int bytes, int flags, void* item);

void Queue::Enqueue(void* item, int count)
{
    Submit(0, count * 4, 0, item);
}
