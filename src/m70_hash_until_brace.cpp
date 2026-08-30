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
// MATCHED at /O2, 14 of 14 non-relocated words, by NAMING THE CHARACTER IN A
// LOCAL HOISTED OUT OF THE LOOP. It sat at 11 of 14 for a long time, with the
// three wrong words a single two-register swap -- retail walked in r10 and
// read the next character into r9, and this file did the reverse:
//
//      want  mr r10,r3 / lbzu r9,1(r10) / extsb r11,r9
//      got   mr r9,r3  / lbzu r10,1(r9) / extsb r11,r10
//
// THE LEVER, and it generalises. Physical registers are handed out in the
// order the VALUES ARE CREATED, descending r11, r10, r9, r8. The target
// creates the pointer copy SECOND, between the peeled character and the
// loop's own byte:
//
//      target  peel byte = r11, ptr copy = r10, loop byte = r9, h*131 = r8
//      ours    peel byte = r11, loop byte = r10, ptr copy = r9, h*131 = r8
//
// With the read spelled `*c` inside the loop condition there is no named
// character, so the loop's byte temp is created before the pointer copy.
// Hoisting the character into a local read once before the loop and once at
// the bottom of the body -- `char ch = *c;` ... `c++; ch = *c;` -- creates the
// pointer first and the numbering falls out. Three spellings of it all match:
// the local declared before `h`, after `h`, and advanced as `ch = *++c`.
//
// So this is the companion to MATCHED.md's un-naming lever, reached from the
// other side: there a pointless `mr` said the source REPEATED an expression;
// here a transposed register pair said the source NAMED one. What the earlier
// note called a residue that "nothing above reaches" was a creation order, and
// a named local is exactly what moves a creation order.
//
// Also measured, and none of it moves the pair:
//   * `u32 h` declared before the pointer -- WORSE, 9 of 15 and 64 bytes: the
//     accumulator loses r3 and the tail call pays an `mr r3,r10`. That is how
//     the declaration order above was settled, so it is not free to change.
//   * /O2 /Os -- much worse, 52 bytes: `extsb.` fuses the extend and the test
//     and the whole loop re-forms, 3 of 13.
//   * the loop lifted into an inlined `static u32 HashUntilBrace(const char*)`
//     so the walk runs on a parameter copy -- byte-identical to the local
//     version, same 11 of 14.
//   * the second terminator as a `break`, `while (*c != 0) { if (*c == '}')
//     break; ... }` -- a different AST that produces the SAME rotation, and
//     byte-identical, same 11 of 14.
//   * the increment folded into the accumulate, `h = h * 131 + *c++;` --
//     byte-identical.
//   * the walk on a `const u8*` with `(char)` at all three reads, per the
//     signedness-split lever of sub_8215A420 -- byte-identical. The load is
//     already `lbz` + `extsb` either way, so the cast changes no vreg.
// The transposition lever from sub_827DAC60 points at the optimisation level,
// and the level is ruled out here.
//
// TOOL NOTE, kept because it cost a wrong number once. Written with the loop
// in a `static` helper the object holds TWO 60-byte functions -- MSVC emits
// the helper as its own COMDAT as well as inlining it -- and match.py then
// refuses to choose without `--sym LookupName`. Scoring the helper instead
// reports 12 of 15 with NO relocated word, which is a better number for the
// wrong function: the helper ends in `blr` where the image has the tail
// branch. The version below is written without the helper, so the object
// holds one function and no `--sym` is needed. The helper form compiles to
// the same bytes.

#include "types.h"

u32 LookupHash(u32 h);

u32 LookupName(const char* name)
{
    const char* c = name;
    u32 h = 0;
    char ch = *c;

    while (ch != 0 && ch != '}')
    {
        h = h * 131 + ch;
        c++;
        ch = *c;
    }

    return LookupHash(h);
}
