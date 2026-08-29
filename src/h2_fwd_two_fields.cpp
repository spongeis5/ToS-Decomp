#include "types.h"

// sub_8219B9A0 -- tail call passing the addresses of two sub-objects of the
// block held at +0x40. 16 B, 4 callers.
//
//      lwz     r11,64(r3)
//      addi    r5,r11,32          ; &block->at20
//      addi    r4,r11,44          ; &block->at2C
//      b       0x82199828
//
// r3 is passed through untouched, so the receiver is the same object. Only
// the branch is relocated: 3 of 4 words are real bytes.
//
// The two `addi`s are emitted r5 BEFORE r4, which is the argument order the
// source is being read as -- the later argument's address is formed first.

struct Block2C
{
    /* 0x00 */ char unk0000[0x20];
    /* 0x20 */ s32  at20;
    /* 0x24 */ char unk0024[0x08];
    /* 0x2C */ s32  at2C;
};
ASSERT_OFFSET(Block2C, at20, 0x20);
ASSERT_OFFSET(Block2C, at2C, 0x2C);

struct HolderBlock
{
    /* 0x00 */ char     unk0000[0x40];
    /* 0x40 */ Block2C* block;
};
ASSERT_OFFSET(HolderBlock, block, 0x40);

int Apply(HolderBlock* h, s32* a, s32* b);

int ApplyBlockFields(HolderBlock* h)
{
    return Apply(h, &h->block->at2C, &h->block->at20);
}
