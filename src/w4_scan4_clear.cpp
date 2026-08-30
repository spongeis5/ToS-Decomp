#include "types.h"

// sub_82226F38 -- scan four slots for a key, clear the hit and the key's own
// back-pointer, then tail-call with (r3, r4) untouched. 48 B, 4 callers.
//
//      li      r10,4
//      addi    r11,r3,332       the slot array
//      mtctr   r10
//      li      r10,0            the zero, reused after mtctr latches
// loop:
//      lwz     r9,0(r11)
//      cmplw   cr6,r9,r4        pointer against pointer
//      bne-    cr6,skip
//      stw     r10,0(r11)       slots[i] = 0
//      stw     r10,4(r4)        key->f4 = 0
// skip:
//      addi    r11,r11,4
//      bdnz+   loop
//      b       82214C80         tail call; r3/r4 already hold the arguments

struct Keyed
{
    /* 0x04 */ char unk0000[4];
    /* 0x04 */ void* back;
};

struct SlotRow
{
    /* 0x14C */ char  unk0000[332];
    /* 0x14C */ void* slots[4];
};

ASSERT_OFFSET(SlotRow, slots, 332);

void Tail_82214C80(SlotRow*, Keyed*);

void ClearMatching(SlotRow* r, Keyed* key)
{
    for (int i = 0; i < 4; ++i)
    {
        if (r->slots[i] == key)
        {
            r->slots[i]  = 0;
            key->back = 0;
        }
    }
    Tail_82214C80(r, key);
}
