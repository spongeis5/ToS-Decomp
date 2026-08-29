#include "types.h"

// sub_822038C0 -- store five arguments, clear five scalars and zero a
// two-element array. 60 B, 6 callers.
//
//      li      r11,0
//      stw     r4,4(r3) ; stw r5,8(r3)
//      addi    r10,r3,52                  <-- DEAD: r10 is never read
//      stw     r6,12(r3) ; stw r7,16(r3) ; stw r8,20(r3)
//      stb     r11,24(r3)
//      stw     r11,28(r3) ; stw r11,32(r3) ; stw r11,36(r3)
//      stw     r11,48(r3) ; stw r11,52(r3) ; stw r11,56(r3)
//      blr
//
// Offsets 0, 40 and 44 are skipped, so this is a list of named fields and not
// a memset.
//
// THE DEAD `addi r10,r3,52` IS AN UNROLLED COUNTED LOOP, not an inlined
// helper. That is a third mechanism for the "address computed and never read"
// fingerprint, alongside the two-level inlining of sub_82164040 and
// sub_82703E28, and it is the one that fits here: MSVC materialises the
// array's base for the induction variable, fully unrolls two iterations into
// `52(r3)` and `56(r3)`, and leaves the base behind.
//
// Six shapes were measured against this one word, all at /O2 (/Os changes
// nothing here):
//
//   for (i = 0; i < 2; i++) r->pair[i] = 0;          15 of 15   MATCH
//   TriInit(&r->tri) -> t->a = 0; ZeroPair(&t->b)    14 of 15   addi r3,48
//   TriInit(&r->tri) -> ZeroWord(&t->a);ZeroPair(&t->b) 14/15   addi r3,48
//   ZeroPair(r->pair)                                 3 of 14   no addi
//   NodeInit(&r->node) -> NodeClear(n)                3 of 14   no addi
//   TriInitB -> ZeroWord x3                           3 of 14   no addi
//
// The two-level forms DO leave a dead address, which confirms the mechanism,
// but they leave the OUTER pointer -- r3+48, the sub-object's own base --
// where the target's is r3+52. Only the loop puts it on the pair itself.

struct Record
{
    /* 0x00 */ char unk0000[0x04];
    /* 0x04 */ s32  f04;
    /* 0x08 */ s32  f08;
    /* 0x0C */ s32  f0C;
    /* 0x10 */ s32  f10;
    /* 0x14 */ s32  f14;
    /* 0x18 */ u8   f18;
    /* 0x19 */ char unk0019[0x03];
    /* 0x1C */ s32  f1C;
    /* 0x20 */ s32  f20;
    /* 0x24 */ s32  f24;
    /* 0x28 */ char unk0028[0x08];
    /* 0x30 */ s32  f30;
    /* 0x34 */ s32  pair[2];
};
ASSERT_OFFSET(Record, f04,  0x04);
ASSERT_OFFSET(Record, f18,  0x18);
ASSERT_OFFSET(Record, f30,  0x30);
ASSERT_OFFSET(Record, pair, 0x34);

void InitRecord(Record* r, s32 a, s32 b, s32 c, s32 d, s32 e)
{
    r->f04 = a;
    r->f08 = b;
    r->f0C = c;
    r->f10 = d;
    r->f14 = e;
    r->f18 = 0;
    r->f1C = 0;
    r->f20 = 0;
    r->f24 = 0;
    r->f30 = 0;

    for (int i = 0; i < 2; i++)
        r->pair[i] = 0;
}
