#include "types.h"

// sub_8264B6F0 -- "is the global slot both valid and non-empty". 40 B,
// 13 callers.
//
//      lis     r11,-32092
//      addi    r10,r11,1572     &g_gate                 ; = 82A40624
//      lwz     r11,4(r10)       g_gate.value            ; = 82A40628
//      cmpwi   cr6,r11,0        SIGNED
//      blt-    cr6,zero
//      cmplwi  cr6,r11,0        UNSIGNED, a second compare of the SAME word
//      li      r3,1
//      bnelr   cr6
// zero:li      r3,0
//      blr
//
// Two things are readable here and both cost a word if guessed wrong.
//
// 1. The address is materialised because the field offset is 4. Offset 0
//    folds into the lo half of the load's displacement; a non-zero offset
//    does not, so `lis` + `addi` + a displaced load is a global STRUCT FIELD.
//
// 2. THE SECOND COMPARE IS THE SIGNEDNESS TELL. Written with both tests on
//    one signedness -- `if (v >= 0 && v != 0)` -- MSVC reuses cr6 from the
//    `cmpwi` for the `!= 0` (equality does not care about signedness) and the
//    function is 36 B, otherwise word-identical. A second `cmplwi` on a
//    register that was just compared means the source changed signedness
//    between the two tests. Any of the four spellings that does that matches;
//    an unsigned field with a signed cast on the range test is the one that
//    reads as real code.
//
// The short-circuit `&&` is also load-bearing: written as two separate `if`s
// with their own `return 0`, MSVC turns the second test into the branchless
// `addic`/`subfe` and the function is a different 40 bytes.

struct Gate
{
    /* 0x00 */ s32 unk0000;
    /* 0x04 */ u32 value;
};
ASSERT_OFFSET(Gate, value, 0x04);

extern Gate g_gate;

int GateIsSet(void)
{
    if ((s32)g_gate.value >= 0 && g_gate.value != 0)
        return 1;
    return 0;
}
