// sub_82698E08 -- copy a 16-byte value into the i'th row of a table.
// 60 B, 3 callers.
//
//      addi   r10,r1,32
//      std    r5,32(r1)
//      std    r6,40(r1)
//      rlwinm r11,r4,4,0,27        i * 16
//      add    r11,r11,r3
//      lwz    r7,4(r10)
//      addi   r8,r11,220           DEAD -- r8 is never read
//      lwz    r6,8(r10)
//      lwz    r5,12(r10)
//      lwz    r9,0(r10)
//      stw    r9,220(r11) ; stw r7,224(r11) ; stw r6,228(r11)
//      stw    r5,232(r11)
//
// THE TWO `std`s ARE THE ABI, NOT THE FUNCTION.  A 16-byte struct passed BY
// VALUE arrives in two 64-bit argument registers; the callee spills them to
// its home area and reads the four words back.  Nothing else produces a pair
// of 64-bit stores to the incoming parameter area followed by word loads
// from it -- a pointer parameter would simply be dereferenced.
//
// So the signature is (table, index, value-by-value), the element is 16
// bytes -- `rlwinm ,4` -- and the array starts at +220.
//
// The dead `addi r8,r11,220` is the row's address, computed and never read:
// the marker of an inlined helper that took `&t->rows[i]`, since a flat body
// folds every access into base+offset and never forms the pointer.

#include "types.h"

struct Quad
{
    /* 0x00 */ u32 a;
    /* 0x04 */ u32 b;
    /* 0x08 */ u32 c;
    /* 0x0C */ u32 d;
};
ASSERT_SIZE(Quad, 16);

struct QuadTable
{
    /* 0x0000 */ char unk0000[0xDC];
    /* 0x00DC */ Quad rows[1];
};
ASSERT_OFFSET(QuadTable, rows, 0xDC);

static void StoreQuad(Quad* dst, Quad v)
{
    *dst = v;
}

void SetRow(QuadTable* t, int i, Quad v)
{
    StoreQuad(&t->rows[i], v);
}
