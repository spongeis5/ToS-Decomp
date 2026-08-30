// sub_821FA2B8 -- is this locale string one of the four the title ships?
// 236 bytes, 2 callers.
//
// Four inlined string compares against literals whose addresses the
// annotator resolves for us:
//
//      82000B14  "EN_US"
//      82000AFC  "EN_CA"
//      8200AD08  "FR_CA"
//      820009C0  "ES_MX"
//
// Each is the nine-instruction `strcmp` INTRINSIC and not a hand-written
// loop: BOTH pointers increment (`addi r10,r10,1 ; addi r9,r9,1`) and the
// difference is formed with `subf` on the two loaded bytes rather than by
// the loop-invariant-delta transform a hand-written body gets. That is the
// same reading src/p1_find_name.cpp records, and it is why <string.h> is
// included here.
//
//      lbz   r8,0(r10)      *s
//      lbz   r7,0(r9)       *lit
//      cmpwi cr6,r8,0
//      subf  r8,r7,r8       *s - *lit, so the argument order is (s, lit)
//      beq-  cr6,out
//      addi  r10,r10,1 ; addi r9,r9,1
//      cmpwi cr6,r8,0 ; beq+ cr6,loop
//
// The four `== 0` exits all branch FORWARD to one shared `li r3,1 ; blr` at
// 821FA39C, and the last term's failure falls into `li r3,0 ; bnelr cr6`
// immediately before it. Per MATCHED.md that shared true-exit is the
// short-circuit `||` form; separate `if (...) return 1;` statements each get
// a private `li r3,1 ; blr`.
//
// No trailing `clrlwi` on the result, so the return type is `int` and not
// `bool` -- the `li` pair already produces 0 or 1 and nothing normalises it.
//
// r3 is copied into r11 once and each compare starts from `mr r10,r11`,
// except the last, which walks r11 itself because it is the final use.

#include "types.h"
#include <string.h>

int IsShippedLocale(const char* s)
{
    return strcmp(s, "EN_US") == 0
        || strcmp(s, "EN_CA") == 0
        || strcmp(s, "FR_CA") == 0
        || strcmp(s, "ES_MX") == 0;
}
