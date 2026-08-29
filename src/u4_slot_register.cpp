#include "types.h"

// sub_821A7E58 -- zero two fields, then park `this` in the first free entry of
// an eight-slot global table. 72 B, 6 callers.
//
//      lis     r11,-32102
//      li      r10,0            i = 0
//      addi    r9,r11,4956      r9 = g_slots   (0x829A135C)
//      stw     r10,24(r3)       +0x18 = 0
//      stw     r10,28(r3)       +0x1C = 0
//      mr      r11,r9           the walking pointer
//  L:  lwz     r8,0(r11)
//      cmplwi  cr6,r8,0
//      beq-    cr6,free         -> the cold block, laid out past the return
//      addi    r11,r11,4
//      addi    r8,r9,32         end = g_slots + 8, recomputed each pass
//      addi    r10,r10,1
//      cmpw    cr6,r11,r8       SIGNED -- the induction variable is an int
//      blt+    cr6,L
//      blr                      table full: nothing stored
// free:rlwinm  r11,r10,2,0,29   i * 4
//      stwx    r3,r11,r9        g_slots[i] = this
//      blr
//
// The loop is strength-reduced to a pointer walk against `g_slots + 8`, but
// the INDEX is kept alive alongside it and the store recomputes i * 4 from it
// -- the same split sub_821559D8 and sub_82254A88 have, where the hit block
// sits outside the loop and past the reduction.
//
// 32 bytes of span with a 4-byte stride is eight pointers, so the array bound
// is asserted by the code rather than guessed.

struct Slot821A;

extern Slot821A* g_slots[8];

struct Slot821A
{
    /* 0x00 */ char unk0000[0x18];
    /* 0x18 */ s32  f018;
    /* 0x1C */ s32  f01C;
};
ASSERT_OFFSET(Slot821A, f018, 0x18);
ASSERT_OFFSET(Slot821A, f01C, 0x1C);

void RegisterSlot(Slot821A* p)
{
    p->f018 = 0;
    p->f01C = 0;

    for (int i = 0; i < 8; ++i)
    {
        if (g_slots[i] == 0)
        {
            g_slots[i] = p;
            return;
        }
    }
}
