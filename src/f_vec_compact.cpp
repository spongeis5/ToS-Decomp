// sub_826C6C60 -- remove every null pointer from a vector, walking BACKWARDS
// and shifting the tail down over each hole. 112 B, 27 callers.
//
//      lwz     r11,4(r3)               v->count
//      addic.  r8,r11,-1               i = count - 1, sets CR0
//      bltlr                           empty: nothing to do
//  O:  rlwinm  r7,r8,2,0,29            i * 4   (outside the back edge)
//      lwz     r11,0(r3)               v->items          <- loop top
//      lwzx    r10,r7,r11
//      cmplwi  cr6,r10,0
//      bne-    cr6,next
//      lwz     r10,4(r3)
//      mr      r11,r8                  j = i
//      addi    r10,r10,-1
//      stw     r10,4(r3)               --v->count
//      cmpw    cr6,r8,r10
//      bge-    cr6,next
//      mr      r10,r7
//  I:  lwz     r9,0(r3)                v->items, RELOADED every iteration
//      addi    r11,r11,1
//      add     r9,r10,r9
//      addi    r10,r10,4
//      lwz     r6,4(r9)
//      stw     r6,0(r9)                v->items[j] = v->items[j + 1]
//      lwz     r5,4(r3)                v->count, RELOADED every iteration
//      cmpw    cr6,r11,r5
//      blt+    cr6,I
// next:addic.  r8,r8,-1
//      addi    r7,r7,-4
//      bge+    O
//      blr
//
// Both `v->items` and `v->count` are re-read inside the inner loop. The
// store `v->items[j] = ...` writes through a pointer the compiler cannot
// prove disjoint from the Vec itself, so neither field survives it. That is
// the aliasing tell, and it is what says the source subscripts through
// `v->` at every use instead of hoisting a local base pointer.
//
// `addic.` twice, at the guard and on the back edge, is the shape MSVC gives
// a counted `for` with a variable start: a guard in front and a do/while
// underneath. The `rlwinm` for i*4 sits ABOVE the loop top and is maintained
// by `addi r7,r7,-4`, so the subscript was strength-reduced, not written as
// a pointer.
//
// The inner guard compares against the value just stored rather than
// re-reading it -- store-to-load forwarding across `--v->count`.

#include "types.h"

struct Vec
{
    /* 0x00 */ void** items;
    /* 0x04 */ s32    count;
};

ASSERT_OFFSET(Vec, items, 0x00);
ASSERT_OFFSET(Vec, count, 0x04);

void VecCompact(Vec* v)
{
    for (int i = v->count - 1; i >= 0; --i)
    {
        if (v->items[i] == 0)
        {
            --v->count;
            for (int j = i; j < v->count; ++j)
                v->items[j] = v->items[j + 1];
        }
    }
}
