#include "types.h"

// sub_82697748 -- bounded byte compare (memcmp), 56 bytes.
//
// THE ADDRESS IS NOT 82697740. The inventory records one 68-byte row at
// 82697740, and that row covers TWO bodies:
//
//   82697740  b     0x828a8c50    a 4-byte tail-call thunk
//   82697744  .long 0             linker alignment padding between COMDATs
//   82697748  mr    r11,r3        THIS function, 56 bytes
//
// 82697748 is not itself an inventory start -- nothing reaches it with a `bl`
// -- so `tools/match.py` cannot be pointed at it, and pointing it at 82697740
// is exactly the hole match.can_shrink documents: the thunk's only word is a
// relocated branch, so nothing would be verified. See the measurement at the
// bottom for how this file was checked instead.
//
//      mr      r11,r3
//      mr      r10,r4
//      li      r3,0
//      cmplwi  cr6,r5,0
//      beqlr   cr6              n == 0 -> 0
//      add     r9,r11,r5        end = a + n
//  L:  lbz     r8,0(r11)
//      lbz     r7,0(r10)
//      subf.   r3,r7,r8         d = *a - *b, straight into the result register
//      bnelr                    non-zero -> return it
//      addi    r11,r11,1
//      addi    r10,r10,1
//      cmpw    cr6,r11,r9
//      bne+    cr6,L            while (a != end)
//      blr                      r3 is still the last (zero) difference
//
// Both bytes arrive zero-extended through `lbz` with no `extsb`, so the
// subtraction is on unsigned chars -- memcmp semantics, not strcmp's signed
// `char` (compare src/i_strcmp.cpp, which sign-extends both).
//
// The loop top is a branch target reached by FALLING INTO it, with no peeled
// copy of the test in front, so the body is a do/while guarded by the `n == 0`
// test above it.

int MemCompare(const u8* a, const u8* b, u32 n)
{
    int d = 0;

    if (n != 0)
    {
        const u8* end = a + n;

        do
        {
            d = *a - *b;
            if (d != 0)
                break;
            ++a;
            ++b;
        }
        while (a != end);
    }

    return d;
}
