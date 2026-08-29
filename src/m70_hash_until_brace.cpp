// sub_82600960 -- hash a string, stopping at NUL or '}', and tail-call the
// lookup with it. 60 bytes, 3 callers.
//
//      lbz   r11,0(r3) ; mr r10,r3 ; li r3,0
//      extsb r11,r11 ; cmpwi cr6,r11,0 ; beq- cr6,<call>
//  L:  cmpwi cr6,r11,125 ; beq- cr6,<call>
//      mulli r8,r3,131
//      lbzu  r9,1(r10)
//      add   r3,r8,r11
//      extsb r11,r9 ; cmpwi cr6,r11,0 ; bne+ cr6,L
//  call:
//      b     0x82600880
//
// The same 131 hash as src/m31_hash_find_node.cpp, with a second terminator
// and the same two tells. `extsb` plus a SIGNED `cmpwi` on every character
// means plain `char`, and `mr r10,r3` means the walk runs on a SEPARATE local
// rather than consuming the parameter -- advancing the parameter itself lets
// MSVC run `lbzu` straight off r3 and the copy disappears.
//
// The rotation names the loop: the `!= 0` test is peeled out in front and
// repeated at the bottom, while the `!= '}'` test sits at the loop TOP, which
// is what `while (c != 0 && c != '}')` gives -- the second term of the
// condition is the only one that has to be re-tested before each body.
//
// 125 is '}'.
//
// The accumulator lives in r3 from `li r3,0` onwards, so it is already in the
// argument register when the tail call is reached -- and DECLARATION ORDER is
// what puts it there. With `u32 h` declared before the pointer, MSVC gives
// the hash r10 and pays an `mr r3,r10` before the branch: 9 of 15 words and
// four bytes too long, every register after the prologue renamed. Declaring
// the pointer first frees r3 for the accumulator. Same lever as
// sub_826C0F50, reached through register assignment rather than an `add`.
//
// The tail branch is relocated, so 14 of 15 words are compared.
//
// NEAR MISS: 11 of 14 at /O2, right length, and the three wrong words are one
// two-register SWAP -- retail walks in r10 and reads the next character into
// r9, we do the reverse:
//
//      want  mr r10,r3 / lbzu r9,1(r10) / extsb r11,r9
//      got   mr r9,r3  / lbzu r10,1(r9) / extsb r11,r10
//
// Everything else, including the loop rotation, the CSE of the accumulator
// into r3 and the multiply's operand registers, is exact.
//
// Measured, and none of it moves the pair:
//   * `u32 h` declared before the pointer -- WORSE, 9 of 15 and 64 bytes: the
//     accumulator loses r3 and the tail call pays an `mr r3,r10`. That is how
//     the declaration order above was settled, so it is not free to change.
//   * /O2 /Os -- much worse, 52 bytes: `extsb.` fuses the extend and the test
//     and the whole loop re-forms, 3 of 13.
//   * the loop lifted into an inlined `static u32 HashUntilBrace(const char*)`
//     so the walk runs on a parameter copy -- byte-identical to the local
//     version, same 11 of 14.
// The transposition lever from sub_827DAC60 points at the optimisation level,
// and the level is ruled out here; what is left is a register name.

#include "types.h"

u32 LookupHash(u32 h);

static u32 HashUntilBrace(const char* c)
{
    u32 h = 0;

    while (*c != 0 && *c != '}')
    {
        h = h * 131 + *c;
        c++;
    }

    return h;
}

u32 LookupName(const char* name)
{
    return LookupHash(HashUntilBrace(name));
}
