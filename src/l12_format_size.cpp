// sub_82674E98 -- bytes for n elements of a format code. 84 B, 4 callers.
//
//      mr      r11,r3            dead: the switch selector, never read again
//      cmpwi   cr6,r3,1   ; beq- one
//      cmpwi   cr6,r3,2   ; beq- two
//      cmpwi   cr6,r3,7   ; ble- zero
//      cmpwi   cr6,r3,9   ; ble- pass
//      cmpwi   cr6,r3,100 ; bne- zero
// pass:mr      r3,r4      ; blr
// zero:li      r3,0       ; blr
// two: addi    r11,r4,1
//      mulli   r11,r11,3
//      rlwinm  r3,r11,0,0,29     & ~3
//      blr
// one: rlwinm  r3,r4,2,0,29      n * 4
//      blr
//
// The decision tree is MSVC's: two equality tests, then a `<= 7` that lands
// on the default, then `<= 9` and `== 100` sharing one body.  8, 9 and 100
// reaching the SAME block is a case GROUP -- MSVC does not invent groups
// (measured on sub_827261D8), so the source wrote them together.
//
// `((n + 1) * 3) & ~3` is align-up-to-4 of `n * 3`, and it is written with
// the addi FIRST because that is the order in the image: `n * 3 + 3` would
// multiply and then add.
//
// mulli BY A SMALL CONSTANT IS THE /Os SIGNATURE.  At /O2 a multiply by 3 is
// rlwinm+add; every one of the ten spellings measured on sub_8280D210 emits
// mulli at /O2 /Os and none does at /O2, so the flag is tried before the
// source.

#include "types.h"

u32 FormatSize(int fmt, u32 n)
{
    switch (fmt)
    {
    case 1:
        return n * 4;

    case 2:
        return ((n + 1) * 3) & ~3u;

    case 8:
    case 9:
    case 100:
        return n;
    }

    return 0;
}
