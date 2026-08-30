// sub_82713100 -- map an IME segment-attribute name to a small integer.
// 332 bytes, 2 callers.
//
// Five inlined `strcmp`s against five adjacent literals:
//
//      82081020  "compositionSegment"   -> 0
//      82081010  "clauseSegment"        -> 1
//      82080FFC  "convertedSegment"     -> 2
//      82080FEC  "phraseLengthAdj"      -> 3
//      82080FDC  "lowConfSegment"       -> 4
//                                 none  -> 5
//
// Same nine-instruction intrinsic as src/y2_locale_known.cpp -- both
// pointers increment and the difference is a `subf` of the two loaded bytes
// -- but the outer shape is the opposite one: every hit has a PRIVATE
// `li r3,N ; blr`, which per MATCHED.md is a sequence of separate `if`
// statements rather than a `||` chain.
//
// `li r3,5` is HOISTED INTO THE ENTRY BLOCK and the last test returns it
// with a bare `bnelr`, so the fall-through default is materialised before
// any of the comparisons run. That is the sub_825BFFF0 shape from
// MATCHED.md seen from the side where MSVC's hoist is the right answer
// rather than the wrong one: with five separate early returns there is
// nothing else for r3 to hold on the way in.
//
// The subject string is read once, `lwz r9,0(r3)` then `lwz r8,0(r9)`, and
// each compare starts from `mr r11,r8`, so the two loads are a single
// source expression named once and the copies are the intrinsic's.
//
// TWO THINGS HAD TO BE RIGHT AND THEY PULL IN OPPOSITE DIRECTIONS.
//
// Five bare `return N;` statements DO NOT put `li r3,5` in the entry block.
// Written that way MSVC materialises nothing up front, folds the last test
// into `cntlzw`/`rlwinm`/`xori`/`addi r3,r11,4`, and comes out 328 bytes and
// 0 of 82. An accumulator initialised before the chain --
// `int kind = 5; if (...) kind = 0; else if (...)` -- is what makes the
// constant live across every comparison, and because the function is
// frameless the assignment-then-common-return tail-duplicates back into the
// `li r3,N ; blr` pairs the image has. 332 bytes, 53 of 73.
//
// The remaining twenty words were all ONE THING: the CONDITION REGISTER
// FIELD. Two of the three compares in each inlined `strcmp` -- the `*a == 0`
// test inside the loop and the outer `== 0` on the difference -- are
// `cmpwi r9,0` on cr0 in the image and `cmpwi cr6,r9,0` at /O2, while the
// third, the loop-back test, is cr6 in both. `/O2 /Os` moves exactly those
// two to cr0 and nothing else: 73 of 73.
//
// That is the same axis MATCHED.md records for `clrlwi.` against
// `clrlwi` + `cmplwi cr6` -- cr0 versus cr6 on an otherwise identical
// instruction stream is an optimisation-level property here, not a source
// shape, and it is worth trying the level before hunting for a spelling.
// The companion note in MATCHED.md says a CR-field difference is "worth
// treating as a naming question"; that holds for a compare the source
// writes, and this is the counter-case for compares the INTRINSIC writes,
// where no naming in the source can reach them.

#include "types.h"
#include <string.h>

struct SegNode
{
    /* 0x00 */ const char* name;
};
ASSERT_OFFSET(SegNode, name, 0x00);

struct SegOwner
{
    /* 0x00 */ SegNode* node;
};
ASSERT_OFFSET(SegOwner, node, 0x00);

int SegmentAttrKind(SegOwner* o)
{
    const char* s = o->node->name;
    int kind = 5;

    if (strcmp(s, "compositionSegment") == 0)
        kind = 0;
    else if (strcmp(s, "clauseSegment") == 0)
        kind = 1;
    else if (strcmp(s, "convertedSegment") == 0)
        kind = 2;
    else if (strcmp(s, "phraseLengthAdj") == 0)
        kind = 3;
    else if (strcmp(s, "lowConfSegment") == 0)
        kind = 4;
    return kind;
}
