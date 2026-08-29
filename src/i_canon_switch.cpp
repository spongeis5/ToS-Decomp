#include "types.h"

// sub_821A99F8 -- a switch that folds a code onto a representative. 36 B,
// 12 callers.
//
//      addi    r11,r3,-27
//      cmplwi  cr6,r11,24
//      bgtlr   cr6              out of range: return the input unchanged
//      lis     r12,0x821A ; addi r12,r12,-26084     table at 821A9A1C
//      rlwinm  r0,r11,2,0,29
//      lwzx    r0,r12,r0
//      mtctr   r0
//      bctr
//
// The 36 bytes are only the dispatch. The 25-entry table at 821A9A1C and the
// six case bodies at 821A9A80..821A9AA8 follow it in .text, in source order:
//
//      821A9A80  li r3,27 ; blr
//      821A9A88  li r3,29 ; blr
//      821A9A90  li r3,31 ; blr
//      821A9A98  li r3,49 ; blr
//      821A9AA0  li r3,51 ; blr
//      821A9AA4  blr                 <- the default, r3 still the input
//
// Read out of the table, the case groups are
//
//      27 28 32 35 -> 27
//      29 30 33 36 -> 29
//      31 34 37 38 -> 31
//      49 -> 49
//      51 -> 51
//      39..48 and 50 -> the default label, i.e. unchanged
//
// The last two are identities written out explicitly; they have their own
// bodies, so the source really does list them. `bgtlr` is the range check
// short-cutting to the same behaviour as the default label, which is why the
// default is a bare `blr` and never needs its own `li`.
//
// cmplwi on `x - 27` is the unsigned range test MSVC always emits for a
// jump table, and says nothing about the signedness of the parameter.
//
// NOT CONFIRMED BY match.py, and the reason is the tool and not the source.
// Our object is 176 bytes -- dispatch, table, bodies, in the retail order --
// against an inventory row of 36, so match.py takes the size-reconciliation
// path. That path is bounded by the next REAL function start, and the
// inventory lists 821A9A1C -- the jump TABLE -- as a function, because the
// word before it is the dispatch's own `bctr` and so passes the
// fall-through test. The window is therefore capped at 36 and match.py
// reports
//
//   9 word(s) compared: 7 identical, 0 differ, 2 differ in a relocated word
//   35 word(s) of length difference not compared
//
// Compared over the full 176 bytes with the same peimage/libmatch the tool
// uses, at /O2: 17 identical, 0 differ, 27 differ in a relocated word -- the
// 27 being the lis/addi pair and all 25 table entries. Every word the
// comparison can speak about agrees. That is evidence, not a match, and this
// file is not a manifest row until match.py or build.py says so.
//
// The level is /O2 and it is informative: at /O2 /Os the compiler switches to
// a BYTE index table with lbzx and the whole thing is 92 bytes, 16 words
// wrong.
int CanonCode(int x)
{
    switch (x)
    {
    case 27:
    case 28:
    case 32:
    case 35:
        return 27;

    case 29:
    case 30:
    case 33:
    case 36:
        return 29;

    case 31:
    case 34:
    case 37:
    case 38:
        return 31;

    case 49:
        return 49;

    case 51:
        return 51;
    }

    return x;
}
