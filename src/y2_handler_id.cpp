// sub_8279B6B0 -- map a function pointer to a small integer id. 224 bytes,
// 4 callers.
//
// Nine comparisons against nine addresses, each of which is a real function
// start in .text (checked: every one begins `mflr r12 ; bl <savegprs>`), so
// the parameter is a FUNCTION POINTER and the constants are `&fn`, formed by
// the usual `lis`/`addi` pair. Both halves of each pair are relocated, so 18
// of the 56 words are supplied by the linker and are excused by match.py.
//
//      lis   r11,-32139
//      addi  r10,r11,-17112        ; = 8274BD28
//      cmplw cr6,r3,r10
//      bne-  cr6,<next>
//      li    r3,1
//      blr
//
// repeated eight times with 1..8, and then the ninth written branchlessly:
//
//      li     r10,9
//      addi   r9,r11,14376         ; = 82743828
//      subf   r8,r3,r9             ; r9 - r3
//      addic  r7,r8,-1             ; carry out iff the difference is non-zero
//      subfe  r5,r6,r6             ; 0 when it was non-zero, -1 when zero
//      and    r3,r5,r10            ; so 9 on equality, 0 otherwise
//
// which is MATCHED.md's `addic rD,rS,-1 ; subfe rT,rD,rS` idiom used to
// select a value rather than to produce a 0/1 -- the mask is the constant 9
// instead of 1. It is the LAST test that is folded this way because it is
// the one whose false arm falls into the shared `return 0`; the eight before
// it each own a private `li r3,N ; blr`, which per MATCHED.md is the
// signature of separate `if` statements rather than a `||` chain.
//
// `subf r8,r3,r9` computes `r9 - r3`, and MSVC emits `a == b` as
// `subf rD,rA,rB`, so the source order of the ninth test is `h == fn`, the
// same order the eight `cmplw`s read.
//
// No trailing `clrlwi`, so the return is `int`-width, not `bool`.

#include "types.h"

typedef void (*Handler)();

void Handler_8274BD28();
void Handler_827464E8();
void Handler_8274DA38();
void Handler_8274CD30();
void Handler_8273F668();
void Handler_827B7358();
void Handler_826E3D30();
void Handler_82742568();
void Handler_82743828();

int HandlerId(Handler h)
{
    if (h == Handler_8274BD28) return 1;
    if (h == Handler_827464E8) return 2;
    if (h == Handler_8274DA38) return 3;
    if (h == Handler_8274CD30) return 4;
    if (h == Handler_8273F668) return 5;
    if (h == Handler_827B7358) return 6;
    if (h == Handler_826E3D30) return 7;
    if (h == Handler_82742568) return 8;
    if (h == Handler_82743828) return 9;
    return 0;
}
